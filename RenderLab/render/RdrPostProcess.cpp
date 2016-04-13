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
		{ RdrShaderSemantic::Position, 0, RdrVertexInputFormat::RG_F32, 0, 0, RdrVertexInputClass::PerVertex, 0 },
		{ RdrShaderSemantic::Texcoord, 0, RdrVertexInputFormat::RG_F32, 0, 8, RdrVertexInputClass::PerVertex, 0 }
	};

	struct QuadVertex
	{
		Vec2 position;
		Vec2 texcoord;
	};

	const RdrVertexShader kQuadVertexShader = { RdrVertexShaderType::Screen, RdrShaderFlags::None };
}

void RdrPostProcess::Init(RdrAssetSystems& rAssets)
{
	m_hToneMapPs = rAssets.shaders.CreatePixelShaderFromFile("p_tonemap.hlsl");
	m_hToneMapConstants = rAssets.resources.CreateStructuredBuffer(nullptr, 1, sizeof(ToneMapParams), RdrResourceUsage::Default);
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

		m_hLumOutputs[i] = rAssets.resources.CreateTexture2D(w, h, RdrResourceFormat::R16G16B16A16_FLOAT);
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

		rDrawState.pComputeShader = rAssets.shaders.GetComputeShader(RdrComputeShader::LuminanceMeasure_First);
		rDrawState.csResources[0] = pLumInput->resourceView;
		rDrawState.csUavs[0] = pLumOutput->uav;
		pRdrContext->DispatchCompute(rDrawState, w, h, 1);

		uint i = 1;
		rDrawState.pComputeShader = rAssets.shaders.GetComputeShader(RdrComputeShader::LuminanceMeasure_Mid);
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

		rDrawState.pComputeShader = rAssets.shaders.GetComputeShader(RdrComputeShader::LuminanceMeasure_Final);
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
		rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		
		rDrawState.psResources[1] = rAssets.resources.GetResource(m_hToneMapConstants)->resourceView;

		// Input assembly
		rDrawState.inputLayout.pInputLayout = nullptr;
		rDrawState.eTopology = RdrTopology::TriangleList;

		rDrawState.vertexBuffers[0].pBuffer = nullptr;
		rDrawState.vertexStride = 0;
		rDrawState.vertexOffset = 0;
		rDrawState.vertexCount = 3;

		pRdrContext->Draw(rDrawState);

		rDrawState.Reset();
	}
	pRdrContext->EndEvent(); // Tonemap

	pRdrContext->EndEvent(); // Post-processing
}
