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
	const RdrVertexShader kScreenVertexShader = { RdrVertexShaderType::Screen, RdrShaderFlags::None };

	float calcLuminance(Vec4 color)
	{
		// https://en.wikipedia.org/wiki/Relative_luminance
		const Vec3 kLumScalar(0.2126f, 0.7152f, 0.0722f);
		return color.x * kLumScalar.x + color.y * kLumScalar.y + color.z * kLumScalar.z;
	}

	inline int getDbgReadIndex(int dbgFrame)
	{
		// Find which debug resource to read from this frame.  We have to wait 2 frames to avoid stalls on readbacks
		int dbgReadIdx = (dbgFrame - 2);
		if (dbgReadIdx < 0)
		{
			dbgReadIdx = 3 + dbgReadIdx;
		}
		return dbgReadIdx;
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
			rOutData.adaptedLum = tonemap.adaptedLum;
		}
	}

	uint getThreadGroupCount(uint dataSize, uint workSize)
	{
		uint count = dataSize / workSize;
		if (dataSize % workSize != 0)
			++count;
		return count;
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
	pTonemapSettings->bloomThreshold = 1.5f;
	m_hToneMapInputConstants = RdrResourceSystem::CreateConstantBuffer(pTonemapSettings, sizeof(ToneMapInputParams), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
}

void RdrPostProcess::HandleResize(uint width, uint height)
{
	// Update luminance measure textures
	uint i = 0;
	uint w = width;
	uint h = height;
	while (w > 16 || h > 16)
	{
		if (m_hLumOutputs[i])
			RdrResourceSystem::ReleaseResource(m_hLumOutputs[i]);

		w = (uint)ceil(w / 16.f);
		h = (uint)ceil(h / 16.f);

		m_hLumOutputs[i] = RdrResourceSystem::CreateTexture2D(w, h, RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
		++i;
	}

	// Bloom downsizing textures
	w = width;
	h = height;
	for (i = 0; i < ARRAYSIZE(m_bloomBuffers); ++i)
	{
		BloomBuffer& rBloom = m_bloomBuffers[i];

		w /= 2;
		h /= 2;

		for (int n = 0; n < ARRAYSIZE(rBloom.hResources); ++n)
		{
			if (rBloom.hResources[n])
				RdrResourceSystem::ReleaseResource(rBloom.hResources[n]);
			rBloom.hResources[n] = RdrResourceSystem::CreateTexture2D(w, h, RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceUsage::Default);
		}

		if (rBloom.hAddConstants)
			RdrResourceSystem::ReleaseConstantBuffer(rBloom.hAddConstants);
		uint constantsSize = RdrConstantBuffer::GetRequiredSize(sizeof(AddParams));
		AddParams* pAddParams = (AddParams*)RdrTransientMem::AllocAligned(constantsSize, 16);
		pAddParams->width = (float)w;
		pAddParams->height = (float)h;
		rBloom.hAddConstants = RdrResourceSystem::CreateConstantBuffer(pAddParams, sizeof(AddParams), RdrCpuAccessFlags::None, RdrResourceUsage::Immutable);
	}
}

void RdrPostProcess::DoPostProcessing(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Post-Process");

	pRdrContext->SetBlendState(false);

	m_dbgFrame = (m_dbgFrame + 1) % 3;

	if (m_debugger.IsActive())
	{
		int dbgReadIdx = getDbgReadIndex(m_dbgFrame);
		dbgLuminanceInput(pRdrContext, pColorBuffer, m_lumDebugRes[m_dbgFrame], m_lumDebugRes[dbgReadIdx], m_debugData);
	}

	DoLuminanceMeasurement(pRdrContext, rDrawState, pColorBuffer);

	DoBloom(pRdrContext, rDrawState, pColorBuffer);

	DoTonemap(pRdrContext, rDrawState, pColorBuffer);

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoLuminanceMeasurement(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Lum Measurement");
	
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
	rDrawState.csResources[0] = pLumOutput->resourceView;
	rDrawState.csUavs[0] = pTonemapOutput->uav;
	pRdrContext->DispatchCompute(rDrawState, 1, 1, 1);

	rDrawState.Reset();

	if (m_debugger.IsActive())
	{
		int dbgReadIdx = getDbgReadIndex(m_dbgFrame);
		dbgTonemapOutput(pRdrContext, pTonemapOutput, m_tonemapDebugRes[m_dbgFrame], m_tonemapDebugRes[dbgReadIdx], m_debugData);
	}

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoBloom(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Bloom");

	// High-pass filter and downsample to 1/2, 1/4, 1/8, and 1/16 res
	{
		uint w = pColorBuffer->texInfo.width / 16;
		uint h = pColorBuffer->texInfo.height / 16;

		const RdrResource* pLumInput = pColorBuffer;
		const RdrResource* pTonemapBuffer = RdrResourceSystem::GetResource(m_hToneMapOutputConstants);
		const RdrResource* pBloomOutput2 = RdrResourceSystem::GetResource(m_bloomBuffers[0].hResources[0]);
		const RdrResource* pBloomOutput4 = RdrResourceSystem::GetResource(m_bloomBuffers[1].hResources[0]);
		const RdrResource* pBloomOutput8 = RdrResourceSystem::GetResource(m_bloomBuffers[2].hResources[0]);
		const RdrResource* pBloomOutput16 = RdrResourceSystem::GetResource(m_bloomBuffers[3].hResources[0]);

		rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::BloomShrink);
		rDrawState.csResources[0] = pLumInput->resourceView;
		rDrawState.csResources[1] = pTonemapBuffer->resourceView;
		rDrawState.csUavs[0] = pBloomOutput2->uav;
		rDrawState.csUavs[1] = pBloomOutput4->uav;
		rDrawState.csUavs[2] = pBloomOutput8->uav;
		rDrawState.csUavs[3] = pBloomOutput16->uav;
		rDrawState.csConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(m_hToneMapInputConstants)->bufferObj;
		rDrawState.csConstantBufferCount = 1;
		pRdrContext->DispatchCompute(rDrawState, w, h, 1);
	}
	
	const RdrResource* pInput1 = nullptr;
	const RdrResource* pInput2 = nullptr;
	const RdrResource* pOutput = nullptr;
	for (int i = ARRAYSIZE(m_bloomBuffers) - 1; i >= 0; --i)
	{
		uint w = pColorBuffer->texInfo.width / 16;
		uint h = pColorBuffer->texInfo.height / 16;

		int blurTargetIdx = (i == 0);
		// Blur
		{ 
			// Vertical blur
			pInput1 = RdrResourceSystem::GetResource(m_bloomBuffers[i].hResources[!blurTargetIdx]);
			pOutput = RdrResourceSystem::GetResource(m_bloomBuffers[i].hResources[blurTargetIdx]);

			uint w = getThreadGroupCount(pOutput->texInfo.width, BLUR_THREAD_COUNT);
			uint h = getThreadGroupCount(pOutput->texInfo.height, BLUR_TEXELS_PER_THREAD);

			rDrawState.Reset();
			rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::BlurVertical);
			rDrawState.csResources[0] = pInput1->resourceView;
			rDrawState.csUavs[0] = pOutput->uav;
			rDrawState.csConstantBufferCount = 0;
			pRdrContext->DispatchCompute(rDrawState, w, h, 1);
			rDrawState.Reset();

			// horizontal blur
			const RdrResource* pTemp = pInput1;
			pInput1 = pOutput;
			pOutput = pTemp;

			w = getThreadGroupCount(pOutput->texInfo.width, BLUR_TEXELS_PER_THREAD);
			h = getThreadGroupCount(pOutput->texInfo.height, BLUR_THREAD_COUNT);

			rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::BlurHorizontal);
			rDrawState.csResources[0] = pInput1->resourceView;
			rDrawState.csUavs[0] = pOutput->uav;
			rDrawState.csConstantBufferCount = 0;
			pRdrContext->DispatchCompute(rDrawState, w, h, 1);
			rDrawState.Reset();
		}

		// Add
		if (i != 0)
		{
			pInput1 = RdrResourceSystem::GetResource(m_bloomBuffers[i - 1].hResources[0]);
			pInput2 = pOutput;
			pOutput = RdrResourceSystem::GetResource(m_bloomBuffers[i - 1].hResources[1]);

			uint w = getThreadGroupCount(pOutput->texInfo.width, ADD_THREADS_X);
			uint h = getThreadGroupCount(pOutput->texInfo.height, ADD_THREADS_Y);

			rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::Add);
			rDrawState.csResources[0] = pInput1->resourceView;
			rDrawState.csSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
			rDrawState.csResources[1] = pInput2->resourceView;
			rDrawState.csUavs[0] = pOutput->uav;
			rDrawState.csConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(m_bloomBuffers[i-1].hAddConstants)->bufferObj;
			rDrawState.csConstantBufferCount = 1;
			pRdrContext->DispatchCompute(rDrawState, w, h, 1);
			rDrawState.Reset();
		}
	}

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoTonemap(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Tonemap");
	
	RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(RdrResourceSystem::kPrimaryRenderTargetHandle);
	RdrDepthStencilView depthView = { 0 };
	pRdrContext->SetRenderTargets(1, &renderTarget, depthView);

	// Vertex shader
	rDrawState.pVertexShader = RdrShaderSystem::GetVertexShader(kScreenVertexShader);

	// Pixel shader
	rDrawState.pPixelShader = RdrShaderSystem::GetPixelShader(m_hToneMapPs);
	rDrawState.psResources[0] = pColorBuffer->resourceView;
	rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
	rDrawState.psResources[1] = RdrResourceSystem::GetResource(m_bloomBuffers[0].hResources[1])->resourceView;
	rDrawState.psSamplers[1] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
	rDrawState.psResources[2] = RdrResourceSystem::GetResource(m_hToneMapOutputConstants)->resourceView;

	// Input assembly
	rDrawState.inputLayout.pInputLayout = nullptr;
	rDrawState.eTopology = RdrTopology::TriangleList;

	rDrawState.vertexBuffers[0].pBuffer = nullptr;
	rDrawState.vertexStride = 0;
	rDrawState.vertexOffset = 0;
	rDrawState.vertexCount = 3;

	pRdrContext->Draw(rDrawState);

	rDrawState.Reset();

	pRdrContext->EndEvent();
}