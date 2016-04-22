#include "Precompiled.h"
#include "RdrPostProcess.h"
#include "RdrContext.h"
#include "RdrDrawState.h"
#include "RdrTransientMem.h"
#include "Renderer.h"
#include "RdrShaderConstants.h"
#include "input/Input.h"

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

	float calcLuminance(Vec4 color)
	{
		// https://en.wikipedia.org/wiki/Relative_luminance
		const Vec3 kLumScalar(0.2126f, 0.7152f, 0.0722f);
		return color.x * kLumScalar.x + color.y * kLumScalar.y + color.z * kLumScalar.z;
	}

	void dbgLuminanceInput(RdrContext* pRdrContext, const RdrResource* pColorBuffer, RdrResource& rLumCopyRes, RdrResource& rLumReadRes, RdrPostProcessDbgData& rOutData)
	{
		int mx, my;
		Input::GetMousePos(mx, my);

		RdrBox srcRect(mx, my, 0, 1, 1, 1);
		if (!rLumCopyRes.pResource)
		{
			RdrTextureInfo info = { 0 };
			info.width = 1;
			info.height = 1;
			info.format = pColorBuffer->texInfo.format;
			info.arraySize = 1;
			info.mipLevels = 1;
			info.sampleCount = 1;
			pRdrContext->CreateTexture(nullptr, info, RdrResourceUsage::Staging, rLumCopyRes);
		}
		pRdrContext->CopyResourceRegion(*pColorBuffer, srcRect, rLumCopyRes, IVec3::kZero);

		if (rLumReadRes.pResource)
		{
			const uint pixelByteSize = 8;
			char dataBuffer[32];
			void* pAlignedData = dataBuffer;
			size_t space;
			std::align(16, pixelByteSize, pAlignedData, space);

			uint size = rdrGetTexturePitch(1, pColorBuffer->texInfo.format);
			pRdrContext->ReadResource(rLumReadRes, pAlignedData, pixelByteSize);

			rOutData.lumColor = Maths::convertHalfToSinglePrecision4((float16*)pAlignedData);
			rOutData.lumAtCursor = calcLuminance(rOutData.lumColor);
		}
	}

	void dbgTonemapOutput(RdrContext* pRdrContext, const RdrResource* pTonemapOutputBuffer, RdrResource& rTonemapCopyRes, RdrResource& rTonemapReadRes, RdrPostProcessDbgData& rOutData)
	{
		RdrBox srcRect(0, 0, 0, sizeof(ToneMapOutputParams), 1, 1);
		if (!rTonemapCopyRes.pResource)
		{
			pRdrContext->CreateStructuredBuffer(nullptr, 1, srcRect.width, RdrResourceUsage::Staging, rTonemapCopyRes);
		}
		pRdrContext->CopyResourceRegion(*pTonemapOutputBuffer, srcRect, rTonemapCopyRes, IVec3::kZero);

		if (rTonemapReadRes.pResource)
		{
			ToneMapOutputParams tonemap;
			pRdrContext->ReadResource(rTonemapReadRes, &tonemap, sizeof(tonemap));
			rOutData.linearExposure = tonemap.linearExposure;
			rOutData.avgLum = tonemap.lumAvg;
		}
	}
}

void RdrPostProcess::Init()
{
	m_debugger.Init(this);

	m_hToneMapPs = RdrShaderSystem::CreatePixelShaderFromFile("p_tonemap.hlsl");
	m_hToneMapOutputConstants = RdrResourceSystem::CreateStructuredBuffer(nullptr, 1, sizeof(ToneMapOutputParams), RdrResourceUsage::Default);

	uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(ToneMapInputParams));
	ToneMapInputParams* pTonemapSettings = (ToneMapInputParams*)RdrTransientMem::AllocAligned(constantsSize, 16);
	pTonemapSettings->white = 4.f;
	pTonemapSettings->middleGrey = 0.7f;
	m_hToneMapInputConstants = RdrResourceSystem::CreateConstantBuffer(pTonemapSettings, sizeof(ToneMapInputParams), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
}

void RdrPostProcess::HandleResize(uint width, uint height)
{
	// Update luminance measure textures
	uint i = 0;
	for (i = 0; i < ARRAYSIZE(m_hLumOutputs); ++i)
	{
		if (m_hLumOutputs[i])
			RdrResourceSystem::ReleaseResource(m_hLumOutputs[i]);
	}

	uint w = width;
	uint h = height;
	i = 0;
	while (w > 16 || h > 16)
	{
		w = (uint)ceil(w / 16.f);
		h = (uint)ceil(h / 16.f);

		m_hLumOutputs[i] = RdrResourceSystem::CreateTexture2D(w, h, RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
		++i;
	}
}

void RdrPostProcess::DoPostProcessing(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Post-Process");

	pRdrContext->SetBlendState(false);

	m_dbgFrame = (m_dbgFrame + 1) % 3;

	// Find which debug resource to read from this frame.  We have to wait 2 frames to avoid stalls on readbacks
	int dbgReadIdx = (m_dbgFrame - 2);
	if (dbgReadIdx < 0)
	{
		dbgReadIdx = 3 + dbgReadIdx;
	}

	if (m_debugger.IsActive())
		dbgLuminanceInput(pRdrContext, pColorBuffer, m_lumDebugRes[m_dbgFrame], m_lumDebugRes[dbgReadIdx], m_debugData);

	pRdrContext->BeginEvent(L"Lum Measurement");
	{
		uint w = pColorBuffer->texInfo.width / 16;
		uint h = pColorBuffer->texInfo.height / 16;

		const RdrResource* pLumInput = pColorBuffer;
		const RdrResource* pLumOutput = RdrResourceSystem::GetResource(m_hLumOutputs[0]);
		const RdrResource* pTonemapOutput = RdrResourceSystem::GetResource(m_hToneMapOutputConstants);

		rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::LuminanceMeasure_First);
		rDrawState.csResources[0] = pLumInput->resourceView;
		rDrawState.csUavs[0] = pLumOutput->uav;
		rDrawState.csConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(m_hToneMapInputConstants)->bufferObj;
		rDrawState.csConstantBufferCount = 1;
		pRdrContext->DispatchCompute(rDrawState, w, h, 1);

		uint i = 1;
		rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::LuminanceMeasure_Mid);
		while (w > 16 || h > 16)
		{
			pLumInput = pLumOutput;
			pLumOutput = RdrResourceSystem::GetResource(m_hLumOutputs[i]);

			rDrawState.csResources[0] = pLumInput->resourceView;
			rDrawState.csUavs[0] = pLumOutput->uav;

			w = (uint)ceil(w / 16.f);
			h = (uint)ceil(h / 16.f);
			++i;

			pRdrContext->DispatchCompute(rDrawState, w, h, 1);
		}

		rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::LuminanceMeasure_Final);
		rDrawState.csUavs[0] = pTonemapOutput->uav;
		pRdrContext->DispatchCompute(rDrawState, 1, 1, 1);

		if (m_debugger.IsActive())
			dbgTonemapOutput(pRdrContext, pTonemapOutput, m_tonemapDebugRes[m_dbgFrame], m_tonemapDebugRes[dbgReadIdx], m_debugData);
	}
	pRdrContext->EndEvent(); // lum measurement

	// todo: bloom

	// Apply tonemap
	// todo: compare compute shader performance vs full screen quad
	pRdrContext->BeginEvent(L"Tonemap");
	{
		RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(RdrResourceSystem::kPrimaryRenderTargetHandle);
		RdrDepthStencilView depthView = { 0 };
		pRdrContext->SetRenderTargets(1, &renderTarget, depthView);

		// Vertex shader
		rDrawState.pVertexShader = RdrShaderSystem::GetVertexShader(kQuadVertexShader);

		// Pixel shader
		rDrawState.pPixelShader = RdrShaderSystem::GetPixelShader(m_hToneMapPs);
		rDrawState.psResources[0] = pColorBuffer->resourceView;
		rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		
		rDrawState.psResources[1] = RdrResourceSystem::GetResource(m_hToneMapOutputConstants)->resourceView;

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
