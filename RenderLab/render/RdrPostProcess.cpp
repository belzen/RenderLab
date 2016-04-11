#include "Precompiled.h"
#include "RdrPostProcess.h"
#include "RdrContext.h"
#include "RdrDrawState.h"
#include "RdrTransientMem.h"
#include "Renderer.h"
#include "RdrShaderConstants.h"

namespace
{
	static const RdrVertexInputElement kQuadVertexDesc[] = {
		{ kRdrShaderSemantic_Position, 0, kRdrVertexInputFormat_RG_F32, 0, 0, kRdrVertexInputClass_PerVertex, 0 },
		{ kRdrShaderSemantic_Texcoord, 0, kRdrVertexInputFormat_RG_F32, 0, 8, kRdrVertexInputClass_PerVertex, 0 }
	};

	struct QuadVertex
	{
		Vec2 position;
		Vec2 texcoord;
	};

	const RdrVertexShader kQuadVertexShader = { kRdrVertexShader_Screen, 0 };
}

void RdrPostProcess::Init(RdrAssetSystems& rAssets)
{
	const uint kNumVerts = 6;
	QuadVertex* pVerts = (QuadVertex*)RdrTransientMem::Alloc(sizeof(QuadVertex) * kNumVerts);
	pVerts[0].position = Vec2(-1.f,  1.f); pVerts[0].texcoord = Vec2(0.f, 0.f);
	pVerts[1].position = Vec2( 1.f,  1.f); pVerts[1].texcoord = Vec2(1.f, 0.f);
	pVerts[2].position = Vec2(-1.f, -1.f); pVerts[2].texcoord = Vec2(0.f, 1.f);
	pVerts[3].position = Vec2(-1.f, -1.f); pVerts[3].texcoord = Vec2(0.f, 1.f);
	pVerts[4].position = Vec2( 1.f,  1.f); pVerts[4].texcoord = Vec2(1.f, 0.f);
	pVerts[5].position = Vec2( 1.f, -1.f); pVerts[5].texcoord = Vec2(1.f, 1.f);

	m_hFullScreenQuadGeo = rAssets.geos.CreateGeo(pVerts, sizeof(QuadVertex), kNumVerts, nullptr, 0, Vec3::kZero);
	m_hFullScreenQuadLayout = rAssets.shaders.CreateInputLayout(kQuadVertexShader, kQuadVertexDesc, ARRAYSIZE(kQuadVertexDesc));

	m_hToneMapPs = rAssets.shaders.CreatePixelShaderFromFile("p_tonemap.hlsl");
	m_hToneMapConstants = rAssets.resources.CreateStructuredBuffer(nullptr, 1, sizeof(ToneMapParams), kRdrResourceUsage_Default);
}

void RdrPostProcess::HandleResize(uint width, uint height, RdrAssetSystems& rAssets)
{
	// Update luminance measure textures
	uint i = 0;
	for (i = 0; i < ARRAYSIZE(m_hLumOutputs); ++i)
	{
		if (m_hLumOutputs[i])
			rAssets.resources.ReleaseResource(m_hLumOutputs[i]);
	}

	uint w = width;
	uint h = height;
	i = 0;
	while (w > 16 || h > 16)
	{
		w = (uint)ceil(w / 16.f);
		h = (uint)ceil(h / 16.f);

		m_hLumOutputs[i] = rAssets.resources.CreateTexture2D(w, h, kResourceFormat_R16G16B16A16_FLOAT);
		++i;
	}
}

void RdrPostProcess::DoPostProcessing(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, RdrAssetSystems& rAssets)
{
	pRdrContext->BeginEvent(L"Post-Process");

	pRdrContext->BeginEvent(L"Lum Measurement");
	{
		uint w = pColorBuffer->texInfo.width / 16;
		uint h = pColorBuffer->texInfo.height / 16;

		const RdrResource* pLumInput = pColorBuffer;
		const RdrResource* pLumOutput = rAssets.resources.GetResource(m_hLumOutputs[0]);

		rDrawState.pComputeShader = rAssets.shaders.GetComputeShader(kRdrComputeShader_LuminanceMeasure_First);
		rDrawState.csResources[0] = pLumInput->resourceView;
		rDrawState.csUavs[0] = pLumOutput->uav;
		pRdrContext->DispatchCompute(rDrawState, w, h, 1);

		uint i = 1;
		rDrawState.pComputeShader = rAssets.shaders.GetComputeShader(kRdrComputeShader_LuminanceMeasure_Mid);
		while (w > 16 || h > 16)
		{
			pLumInput = pLumOutput;
			pLumOutput = rAssets.resources.GetResource(m_hLumOutputs[i]);

			rDrawState.csResources[0] = pLumInput->resourceView;
			rDrawState.csUavs[0] = pLumOutput->uav;

			w = (uint)ceil(w / 16.f);
			h = (uint)ceil(h / 16.f);
			++i;

			pRdrContext->DispatchCompute(rDrawState, w, h, 1);
		}

		rDrawState.pComputeShader = rAssets.shaders.GetComputeShader(kRdrComputeShader_LuminanceMeasure_Final);
		rDrawState.csUavs[0] = rAssets.resources.GetResource(m_hToneMapConstants)->uav;
		pRdrContext->DispatchCompute(rDrawState, 1, 1, 1);
	}
	pRdrContext->EndEvent(); // lum measurement

	// todo: bloom

	// Apply tonemap
	// todo: compare compute shader performance vs full screen quad
	pRdrContext->BeginEvent(L"Tonemap");
	{
		RdrRenderTargetView renderTarget = rAssets.resources.GetRenderTargetView(RdrResourceSystem::kPrimaryRenderTargetHandle);
		RdrDepthStencilView depthView = { 0 };
		pRdrContext->SetRenderTargets(1, &renderTarget, depthView);

		// Vertex shader
		rDrawState.pVertexShader = rAssets.shaders.GetVertexShader(kQuadVertexShader);

		// Pixel shader
		rDrawState.pPixelShader = rAssets.shaders.GetPixelShader(m_hToneMapPs);
		rDrawState.psResources[0] = pColorBuffer->resourceView;
		rDrawState.psSamplers[0] = RdrSamplerState(kComparisonFunc_Never, kRdrTexCoordMode_Clamp, false);
		
		rDrawState.psResources[1] = rAssets.resources.GetResource(m_hToneMapConstants)->resourceView;

		// Input assembly
		rDrawState.pInputLayout = rAssets.shaders.GetInputLayout(m_hFullScreenQuadLayout);
		rDrawState.eTopology = kRdrTopology_TriangleList;

		const RdrGeometry* pGeo = rAssets.geos.GetGeo(m_hFullScreenQuadGeo);
		rDrawState.vertexBuffers[0] = *rAssets.geos.GetVertexBuffer(pGeo->hVertexBuffer);
		rDrawState.vertexStride = pGeo->geoInfo.vertStride;
		rDrawState.vertexOffset = 0;
		rDrawState.vertexCount = pGeo->geoInfo.numVerts;

		pRdrContext->Draw(rDrawState);

		rDrawState.Reset();
	}
	pRdrContext->EndEvent(); // Tonemap

	pRdrContext->EndEvent(); // Post-processing
}
