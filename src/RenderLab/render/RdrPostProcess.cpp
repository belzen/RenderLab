#include "Precompiled.h"
#include "RdrPostProcess.h"
#include "RdrContext.h"
#include "RdrDrawState.h"
#include "RdrFrameMem.h"
#include "Renderer.h"
#include "RdrShaderConstants.h"
#include "input/Input.h"
#include "RdrComputeOp.h"

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

	void dbgLuminanceInput(const InputManager& rInputManager, RdrContext* pRdrContext, const RdrResource* pColorBuffer, RdrResource& rLumCopyRes, RdrResource& rLumReadRes, RdrPostProcessDbgData& rOutData)
	{
		int mx, my;
		rInputManager.GetMousePos(mx, my);

		RdrBox srcRect(max(mx,0), max(my,0), 0, 1, 1, 1);
		if (!rLumCopyRes.pResource)
		{
			RdrTextureInfo info = { 0 };
			info.width = 1;
			info.height = 1;
			info.format = pColorBuffer->texInfo.format;
			info.depth = 1;
			info.mipLevels = 1;
			info.sampleCount = 1;
			pRdrContext->CreateTexture(nullptr, info, RdrResourceUsage::Staging, RdrResourceBindings::kNone, rLumCopyRes);
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

	void setupFullscreenDrawState(RdrDrawState& rDrawState)
	{
		// Vertex shader
		rDrawState.pVertexShader = RdrShaderSystem::GetVertexShader(kScreenVertexShader);

		// Input assembly
		rDrawState.inputLayout.pInputLayout = nullptr;
		rDrawState.eTopology = RdrTopology::TriangleList;

		rDrawState.vertexBuffers[0].pBuffer = nullptr;
		rDrawState.vertexStrides[0] = 0;
		rDrawState.vertexOffsets[0] = 0;
		rDrawState.vertexBufferCount = 1;
		rDrawState.vertexCount = 3;
	}
}

void RdrPostProcess::Init(RdrContext* pRdrContext)
{
	m_debugger.Init(this);
	m_useHistogramToneMap = false;

	m_hToneMapPs = RdrShaderSystem::CreatePixelShaderFromFile("p_tonemap.hlsl", nullptr, 0);
	m_hToneMapOutputConstants = g_pRenderer->GetPreFrameCommandList().CreateStructuredBuffer(nullptr, 1, sizeof(ToneMapOutputParams), RdrResourceUsage::Default);

	const char* histogramDefines[] = { "TONEMAP_HISTOGRAM" };
	m_hToneMapHistogramPs = RdrShaderSystem::CreatePixelShaderFromFile("p_tonemap.hlsl", histogramDefines, 1);

	m_toneMapInputConstants = pRdrContext->CreateConstantBuffer(nullptr, sizeof(ToneMapInputParams), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);

	m_hCopyPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_copy.hlsl", nullptr, 0);

	m_ssaoConstants = pRdrContext->CreateConstantBuffer(nullptr, sizeof(SsaoParams), RdrCpuAccessFlags::Write, RdrResourceUsage::Dynamic);
	m_hSsaoGenPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_ssao_gen.hlsl", nullptr, 0);
	m_hSsaoBlurPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_ssao_blur.hlsl", nullptr, 0);
	m_hSsaoApplyPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_ssao_apply.hlsl", nullptr, 0);
}

void RdrPostProcess::HandleResize(uint width, uint height)
{
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetPreFrameCommandList();

	// Update luminance measure textures
	uint i = 0;
	uint w = width;
	uint h = height;
	while (w > 16 || h > 16)
	{
		if (m_hLumOutputs[i])
			rResCommandList.ReleaseResource(m_hLumOutputs[i]);

		w = (uint)ceil(w / 16.f);
		h = (uint)ceil(h / 16.f);

		m_hLumOutputs[i] = rResCommandList.CreateTexture2D(w, h, RdrResourceFormat::R16G16B16A16_FLOAT, 
			RdrResourceUsage::Default, RdrResourceBindings::kUnorderedAccessView, nullptr);
		++i;
	}

	// Bloom downsizing textures
	w = width;
	h = height;
	const uint numBloomBuffers = ARRAY_SIZE(m_bloomBuffers);
	for (i = 0; i < numBloomBuffers; ++i)
	{
		BloomBuffer& rBloom = m_bloomBuffers[i];

		w /= 2;
		h /= 2;

		for (int n = 0; n < ARRAY_SIZE(rBloom.hResources); ++n)
		{
			if (rBloom.hResources[n])
				rResCommandList.ReleaseResource(rBloom.hResources[n]);
			rBloom.hResources[n] = rResCommandList.CreateTexture2D(w, h, RdrResourceFormat::R16G16B16A16_FLOAT, 
				RdrResourceUsage::Default, RdrResourceBindings::kUnorderedAccessView, nullptr);
		}

		if (rBloom.hBlendConstants)
			rResCommandList.ReleaseConstantBuffer(rBloom.hBlendConstants);

		uint constantsSize = sizeof(Blend2dParams);
		Blend2dParams* pBlendParams = (Blend2dParams*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pBlendParams->size1.x = (float)w;
		pBlendParams->size1.y = (float)h;
		if (i == numBloomBuffers - 2)
		{
			pBlendParams->weight1 = 1.f / numBloomBuffers;
			pBlendParams->weight2 = 1.f / numBloomBuffers;
		}
		else
		{
			// Blends after the first one need take the full weight from the smaller buffer.
			pBlendParams->weight1 = 1.f / numBloomBuffers;
			pBlendParams->weight2 = 1.f;
		}

		rBloom.hBlendConstants = rResCommandList.CreateConstantBuffer(pBlendParams, constantsSize, RdrCpuAccessFlags::None, RdrResourceUsage::Immutable);
	}

	if (m_useHistogramToneMap)
	{
		uint numTiles = (uint)ceilf(width / 32.f) * (uint)ceilf(height / 16.f); // todo - share tile sizes & histogram bin sizes with shader
		if (m_hToneMapTileHistograms)
			rResCommandList.ReleaseResource(m_hToneMapTileHistograms);
		m_hToneMapTileHistograms = rResCommandList.CreateDataBuffer(nullptr, numTiles * 64, RdrResourceFormat::R16_UINT, RdrResourceUsage::Default);
	
		if (m_hToneMapMergedHistogram)
			rResCommandList.ReleaseResource(m_hToneMapMergedHistogram);
		m_hToneMapMergedHistogram = rResCommandList.CreateDataBuffer(nullptr, 64, RdrResourceFormat::R32_UINT, RdrResourceUsage::Default);

		if (m_hToneMapHistogramResponseCurve)
			rResCommandList.ReleaseResource(m_hToneMapHistogramResponseCurve);
		m_hToneMapHistogramResponseCurve = rResCommandList.CreateDataBuffer(nullptr, 64, RdrResourceFormat::R16_FLOAT, RdrResourceUsage::Default);


		if (m_hToneMapHistogramSettings)
			rResCommandList.ReleaseConstantBuffer(m_hToneMapHistogramSettings);

		struct Test
		{
			float logLuminanceMin;
			float logLuminanceMax;
			uint tileCount;
		};
		uint constantsSize = sizeof(Test);
		Test* pTest = (Test*)RdrFrameMem::AllocAligned(constantsSize, 16);
		pTest->logLuminanceMin = 0.f;
		pTest->logLuminanceMax = 3.f;
		pTest->tileCount = numTiles;
		m_hToneMapHistogramSettings = rResCommandList.CreateConstantBuffer(pTest, constantsSize, RdrCpuAccessFlags::None, RdrResourceUsage::Default);
	}

	ResizeSsaoResources(width, height);
}

void RdrPostProcess::DoPostProcessing(const InputManager& rInputManager, RdrContext* pRdrContext, RdrDrawState& rDrawState, 
	const RdrActionSurfaces& rBuffers, const AssetLib::PostProcessEffects& rEffects, const RdrGlobalConstants& rGlobalConstants)
{
	const RdrResource* pColorBuffer = RdrResourceSystem::GetResource(rBuffers.colorBuffer.hTexture);

	pRdrContext->BeginEvent(L"Post-Process");

	if (rEffects.ssao.enabled)
	{
		DoSsao(pRdrContext, rDrawState, rBuffers, rEffects, rGlobalConstants);
	}

	m_dbgFrame = (m_dbgFrame + 1) % 3;

	if (m_debugger.IsActive())
	{
		int dbgReadIdx = getDbgReadIndex(m_dbgFrame);
		dbgLuminanceInput(rInputManager, pRdrContext, pColorBuffer, m_lumDebugRes[m_dbgFrame], m_lumDebugRes[dbgReadIdx], m_debugData);
	}

	//////////////////////////////////////////////////////////////////////////
	// Update tonemapping input constants
	ToneMapInputParams tonemapSettings;
	tonemapSettings.white = rEffects.eyeAdaptation.white;
	tonemapSettings.middleGrey = rEffects.eyeAdaptation.middleGrey;
	tonemapSettings.minExposure = pow(2.f, rEffects.eyeAdaptation.minExposure);
	tonemapSettings.maxExposure = pow(2.f, rEffects.eyeAdaptation.maxExposure);
	tonemapSettings.bloomThreshold = rEffects.bloom.threshold;
	tonemapSettings.frameTime = Time::FrameTime() * rEffects.eyeAdaptation.adaptationSpeed; // TODO: Need previous frame's time for this to be correct.
	pRdrContext->UpdateConstantBuffer(m_toneMapInputConstants, &tonemapSettings, sizeof(ToneMapInputParams));

	//////////////////////////////////////////////////////////////////////////
	RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(RdrResourceSystem::kPrimaryRenderTargetHandle);
	RdrDepthStencilView depthView = { 0 };
	pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
	pRdrContext->SetViewport(Rect(0.f, 0.f, (float)pColorBuffer->texInfo.width, (float)pColorBuffer->texInfo.height));
	pRdrContext->SetBlendState(RdrBlendMode::kOpaque);

	//////////////////////////////////////////////////////////////////////////
	if (m_useHistogramToneMap)
	{
		DoLuminanceHistogram(pRdrContext, rDrawState, pColorBuffer);
	}
	else
	{
		DoLuminanceMeasurement(pRdrContext, rDrawState, pColorBuffer);
	}

	const RdrResource* pBloomBuffer = nullptr;
	if (rEffects.bloom.enabled)
	{
		DoBloom(pRdrContext, rDrawState, pColorBuffer);
		pBloomBuffer = RdrResourceSystem::GetResource(m_bloomBuffers[0].hResources[1]);
	}
	else
	{
		pBloomBuffer = RdrResourceSystem::GetDefaultResource(RdrDefaultResource::kBlackTex2d);
	}

	DoTonemap(pRdrContext, rDrawState, pColorBuffer, pBloomBuffer);

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoLuminanceMeasurement(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Lum Measurement");
	
	uint w = RdrComputeOp::getThreadGroupCount(pColorBuffer->texInfo.width, 16);
	uint h = RdrComputeOp::getThreadGroupCount(pColorBuffer->texInfo.height, 16);

	const RdrResource* pLumInput = pColorBuffer;
	const RdrResource* pLumOutput = RdrResourceSystem::GetResource(m_hLumOutputs[0]);
	const RdrResource* pTonemapOutput = RdrResourceSystem::GetResource(m_hToneMapOutputConstants);

	rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::LuminanceMeasure_First);
	rDrawState.csResources[0] = pLumInput->resourceView;
	rDrawState.csUavs[0] = pLumOutput->uav;
	rDrawState.csConstantBuffers[0] = m_toneMapInputConstants;
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

		w = RdrComputeOp::getThreadGroupCount(w, 16);
		h = RdrComputeOp::getThreadGroupCount(h, 16);
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

void RdrPostProcess::DoLuminanceHistogram(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Lum Histogram");

	uint w = RdrComputeOp::getThreadGroupCount(pColorBuffer->texInfo.width, 32);
	uint h = RdrComputeOp::getThreadGroupCount(pColorBuffer->texInfo.height, 16);

	const RdrResource* pLumInput = pColorBuffer;
	const RdrResource* pTileHistograms = RdrResourceSystem::GetResource(m_hToneMapTileHistograms);
	const RdrResource* pMergedHistogram = RdrResourceSystem::GetResource(m_hToneMapMergedHistogram);
	const RdrResource* pResponseCurve = RdrResourceSystem::GetResource(m_hToneMapHistogramResponseCurve);
	const RdrConstantBuffer* pToneMapParams = RdrResourceSystem::GetConstantBuffer(m_hToneMapHistogramSettings);

	rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::LuminanceHistogram_Tile);
	rDrawState.csResources[0] = pColorBuffer->resourceView;
	rDrawState.csUavs[0] = pTileHistograms->uav;
	rDrawState.csConstantBuffers[0] = pToneMapParams->bufferObj;
	rDrawState.csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(rDrawState, w, h, 1);

	rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::LuminanceHistogram_Merge);
	rDrawState.csResources[0] = pTileHistograms->resourceView;
	rDrawState.csUavs[0] = pMergedHistogram->uav;
	rDrawState.csConstantBuffers[0] = pToneMapParams->bufferObj;
	rDrawState.csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(rDrawState, (uint)ceilf(2700.f / 1024.f), 1, 1); //todo thread counts

	rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::LuminanceHistogram_ResponseCurve);
	rDrawState.csResources[0] = pMergedHistogram->resourceView;
	rDrawState.csUavs[0] = pResponseCurve->uav;
	rDrawState.csConstantBuffers[0] = pToneMapParams->bufferObj;
	rDrawState.csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(rDrawState, 1, 1, 1);

	rDrawState.Reset();

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoBloom(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Bloom");

	// High-pass filter and downsample
	const RdrResource* pLumInput = pColorBuffer;
	const RdrResource* pTonemapBuffer = RdrResourceSystem::GetResource(m_hToneMapOutputConstants);
	const RdrResource* pHighPassShrinkOutput = RdrResourceSystem::GetResource(m_bloomBuffers[0].hResources[0]);

	uint w = RdrComputeOp::getThreadGroupCount(pHighPassShrinkOutput->texInfo.width, SHRINK_THREADS_X);
	uint h = RdrComputeOp::getThreadGroupCount(pHighPassShrinkOutput->texInfo.height, SHRINK_THREADS_Y);

	rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::BloomShrink);
	rDrawState.csResources[0] = pLumInput->resourceView;
	rDrawState.csResources[1] = pTonemapBuffer->resourceView;
	rDrawState.csUavs[0] = pHighPassShrinkOutput->uav;
	rDrawState.csConstantBuffers[0] = m_toneMapInputConstants;
	rDrawState.csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(rDrawState, w, h, 1);

	for (int i = 1; i < ARRAY_SIZE(m_bloomBuffers); ++i)
	{
		const RdrResource* pTexInput = RdrResourceSystem::GetResource(m_bloomBuffers[i - 1].hResources[0]);
		const RdrResource* pTexOutput = RdrResourceSystem::GetResource(m_bloomBuffers[i].hResources[0]);

		w = RdrComputeOp::getThreadGroupCount(pTexOutput->texInfo.width, SHRINK_THREADS_X);
		h = RdrComputeOp::getThreadGroupCount(pTexOutput->texInfo.height, SHRINK_THREADS_Y);

		rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::Shrink);
		rDrawState.csResources[0] = pTexInput->resourceView;
		rDrawState.csUavs[0] = pTexOutput->uav;
		rDrawState.csConstantBufferCount = 0;
		pRdrContext->DispatchCompute(rDrawState, w, h, 1);
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Blur and accumulate bloom
	const RdrResource* pInput1 = nullptr;
	const RdrResource* pInput2 = nullptr;
	const RdrResource* pOutput = nullptr;
	for (int i = ARRAY_SIZE(m_bloomBuffers) - 1; i >= 0; --i)
	{
		int blurInputIdx = (i != (ARRAY_SIZE(m_bloomBuffers) - 1));
		// Blur
		{ 
			// Vertical blur
			pInput1 = RdrResourceSystem::GetResource(m_bloomBuffers[i].hResources[blurInputIdx]);
			pOutput = RdrResourceSystem::GetResource(m_bloomBuffers[i].hResources[!blurInputIdx]);

			uint w = RdrComputeOp::getThreadGroupCount(pOutput->texInfo.width, BLUR_THREAD_COUNT);
			uint h = RdrComputeOp::getThreadGroupCount(pOutput->texInfo.height, BLUR_TEXELS_PER_THREAD);

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

			w = RdrComputeOp::getThreadGroupCount(pOutput->texInfo.width, BLUR_TEXELS_PER_THREAD);
			h = RdrComputeOp::getThreadGroupCount(pOutput->texInfo.height, BLUR_THREAD_COUNT);

			rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::BlurHorizontal);
			rDrawState.csResources[0] = pInput1->resourceView;
			rDrawState.csUavs[0] = pOutput->uav;
			rDrawState.csConstantBufferCount = 0;
			pRdrContext->DispatchCompute(rDrawState, w, h, 1);
			rDrawState.Reset();
		}

		// Blend together
		if (i != 0)
		{
			pInput1 = RdrResourceSystem::GetResource(m_bloomBuffers[i - 1].hResources[0]);
			pInput2 = pOutput;
			pOutput = RdrResourceSystem::GetResource(m_bloomBuffers[i - 1].hResources[1]);

			uint w = RdrComputeOp::getThreadGroupCount(pOutput->texInfo.width, BLEND_THREADS_X);
			uint h = RdrComputeOp::getThreadGroupCount(pOutput->texInfo.height, BLEND_THREADS_Y);

			rDrawState.pComputeShader = RdrShaderSystem::GetComputeShader(RdrComputeShader::Blend2d);
			rDrawState.csResources[0] = pInput1->resourceView;
			rDrawState.csSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
			rDrawState.csResources[1] = pInput2->resourceView;
			rDrawState.csUavs[0] = pOutput->uav;
			rDrawState.csConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(m_bloomBuffers[i-1].hBlendConstants)->bufferObj;
			rDrawState.csConstantBufferCount = 1;
			pRdrContext->DispatchCompute(rDrawState, w, h, 1);
			rDrawState.Reset();
		}
	}

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoTonemap(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const RdrResource* pBloomBuffer)
{
	pRdrContext->BeginEvent(L"Tonemap");
	setupFullscreenDrawState(rDrawState);

	// Pixel shader
	if (m_useHistogramToneMap)
	{
		rDrawState.pPixelShader = RdrShaderSystem::GetPixelShader(m_hToneMapHistogramPs);

		rDrawState.psResources[0] = pColorBuffer->resourceView;
		rDrawState.psResources[1] = pBloomBuffer ? pBloomBuffer->resourceView : RdrShaderResourceView();
		rDrawState.psResources[2] = RdrResourceSystem::GetResource(m_hToneMapOutputConstants)->resourceView;
		rDrawState.psResources[3] = RdrResourceSystem::GetResource(m_hToneMapHistogramResponseCurve)->resourceView;
		rDrawState.psResourceCount = 4;

		rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		rDrawState.psSamplerCount = 1;

		rDrawState.psConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(m_hToneMapHistogramSettings)->bufferObj;
		rDrawState.psConstantBufferCount = 1;
	}
	else
	{
		rDrawState.pPixelShader = RdrShaderSystem::GetPixelShader(m_hToneMapPs);
		rDrawState.psResources[0] = pColorBuffer->resourceView;
		rDrawState.psResources[1] = pBloomBuffer ? pBloomBuffer->resourceView : RdrShaderResourceView();
		rDrawState.psResources[2] = RdrResourceSystem::GetResource(m_hToneMapOutputConstants)->resourceView;
		rDrawState.psResourceCount = 3;

		rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		rDrawState.psSamplerCount = 1;
	}

	pRdrContext->Draw(rDrawState, 1);
	pRdrContext->EndEvent();

	rDrawState.Reset();
}


void RdrPostProcess::ResizeSsaoResources(uint width, uint height)
{
	//http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
	RdrResourceCommandList& rResCommands = g_pRenderer->GetPreFrameCommandList();
	const UVec2 ssaoBufferSize = UVec2(width / 2, height / 2);

	if (m_hSsaoNoiseTexture)
		rResCommands.ReleaseResource(m_hSsaoNoiseTexture);

	// Noise texture
	const int kNoiseTexSize = 4;
	const int kNoiseNumPixels = kNoiseTexSize * kNoiseTexSize;
	char* pNoiseTexData = (char*)RdrFrameMem::AllocAligned(sizeof(char) * 2 * kNoiseNumPixels, 16);
	for (int i = 0; i < kNoiseNumPixels; ++i)
	{
		Vec3 v = Vec3(
			randFloatRange(-1.f, 1.f),
			randFloatRange(-1.f, 1.f),
			0.f);
		v = Vec3Normalize(v);

		pNoiseTexData[i * 2 + 0] = (char)(v.x * 127) + 128;
		pNoiseTexData[i * 2 + 1] = (char)(v.y * 127) + 128;
	}

	m_hSsaoNoiseTexture = rResCommands.CreateTexture2D(kNoiseTexSize, kNoiseTexSize, RdrResourceFormat::R8G8_UNORM,
		RdrResourceUsage::Immutable, RdrResourceBindings::kNone, pNoiseTexData);

	// Sample kernel
	m_ssaoParams.texelSize = float2(1.f / ssaoBufferSize.x, 1.f / ssaoBufferSize.y);
	m_ssaoParams.blurSize = kNoiseTexSize;
	m_ssaoParams.noiseUvScale.x = width / (float)kNoiseTexSize;
	m_ssaoParams.noiseUvScale.y = height / (float)kNoiseTexSize;
	for (int i = 0; i < ARRAY_SIZE(m_ssaoParams.sampleKernel); ++i)
	{
		// Generate random sample point in the hemisphere
		Vec3 v = Vec3(
			randFloatRange(-1.f, 1.f),
			randFloatRange(-1.f, 1.f),
			randFloatRange(0.f, 1.f));
		v = Vec3Normalize(v);

		// Push sample points further out as the index grows.
		float scale = i / (float)ARRAY_SIZE(m_ssaoParams.sampleKernel);
		scale = lerp(0.1f, 1.f, scale * scale);
		v *= scale;

		m_ssaoParams.sampleKernel[i].x = v.x;
		m_ssaoParams.sampleKernel[i].y = v.y;
		m_ssaoParams.sampleKernel[i].z = v.z;
		m_ssaoParams.sampleKernel[i].w = 0.f;
	}

	// Buffers
	rResCommands.ReleaseRenderTarget2d(m_ssaoBuffer);
	m_ssaoBuffer = rResCommands.InitRenderTarget2d(ssaoBufferSize.x, ssaoBufferSize.y, RdrResourceFormat::R8_UNORM, 1);

	rResCommands.ReleaseRenderTarget2d(m_ssaoBlurredBuffer);
	m_ssaoBlurredBuffer = rResCommands.InitRenderTarget2d(ssaoBufferSize.x, ssaoBufferSize.y, RdrResourceFormat::R8_UNORM, 1);
}

void RdrPostProcess::DoSsao(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrActionSurfaces& rBuffers, 
	const AssetLib::PostProcessEffects& rEffects, const RdrGlobalConstants& rGlobalConstants)
{
	const RdrResource* pSsaoBuffer = RdrResourceSystem::GetResource(m_ssaoBuffer.hTexture);
	const RdrResource* pSsaoBlurredBuffer = RdrResourceSystem::GetResource(m_ssaoBlurredBuffer.hTexture);
	const RdrResource* pDepthBuffer = RdrResourceSystem::GetResource(rBuffers.hDepthBuffer);
	const RdrResource* pAlbedoBuffer = RdrResourceSystem::GetResource(rBuffers.albedoBuffer.hTexture);
	const RdrResource* pNormalBuffer = RdrResourceSystem::GetResource(rBuffers.normalBuffer.hTexture);
	const RdrResource* pNoiseBuffer = RdrResourceSystem::GetResource(m_hSsaoNoiseTexture);

	pRdrContext->BeginEvent(L"SSAO");

	m_ssaoParams.sampleRadius = rEffects.ssao.sampleRadius;
	pRdrContext->UpdateConstantBuffer(m_ssaoConstants, &m_ssaoParams, sizeof(m_ssaoParams));

	// Generate SSAO buffer
	{
		RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(m_ssaoBuffer.hRenderTarget);
		RdrDepthStencilView depthView = { 0 };
		pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
		pRdrContext->SetViewport(Rect(0.f, 0.f, (float)pSsaoBuffer->texInfo.width, (float)pSsaoBuffer->texInfo.height));

		setupFullscreenDrawState(rDrawState);

		// Pixel shader
		rDrawState.pPixelShader = RdrShaderSystem::GetPixelShader(m_hSsaoGenPixelShader);
		rDrawState.psResources[0] = pDepthBuffer->resourceView;
		rDrawState.psResources[1] = pNormalBuffer->resourceView;
		rDrawState.psResources[2] = pNoiseBuffer->resourceView;
		rDrawState.psResourceCount = 3;

		rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		rDrawState.psSamplers[1] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
		rDrawState.psSamplerCount = 2;

		rDrawState.psConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hPsPerAction)->bufferObj;
		rDrawState.psConstantBuffers[1] = m_ssaoConstants;
		rDrawState.psConstantBufferCount = 2;

		pRdrContext->Draw(rDrawState, 1);
	}

	// Blur
	{
		RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(m_ssaoBlurredBuffer.hRenderTarget);
		RdrDepthStencilView depthView = { 0 };
		Vec2 viewportSize = g_pRenderer->GetViewportSize();
		pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
		pRdrContext->SetViewport(Rect(0.f, 0.f, (float)pSsaoBlurredBuffer->texInfo.width, (float)pSsaoBlurredBuffer->texInfo.height));

		setupFullscreenDrawState(rDrawState);

		// Pixel shader
		rDrawState.pPixelShader = RdrShaderSystem::GetPixelShader(m_hSsaoBlurPixelShader);
		rDrawState.psResources[0] = pSsaoBuffer->resourceView;
		rDrawState.psResourceCount = 1;

		rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		rDrawState.psSamplerCount = 1;

		rDrawState.psConstantBuffers[0] = m_ssaoConstants;
		rDrawState.psConstantBufferCount = 1;

		pRdrContext->Draw(rDrawState, 1);
	}

	// Apply ambient occlusion
	{
		RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(rBuffers.colorBuffer.hRenderTarget);
		RdrDepthStencilView depthView = { 0 };
		Vec2 viewportSize = g_pRenderer->GetViewportSize();
		pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
		pRdrContext->SetViewport(Rect(0.f, 0.f, viewportSize.x, viewportSize.y));
		pRdrContext->SetBlendState(RdrBlendMode::kSubtractive);

		setupFullscreenDrawState(rDrawState);

		// Pixel shader
		rDrawState.pPixelShader = RdrShaderSystem::GetPixelShader(m_hSsaoApplyPixelShader);
		rDrawState.psResources[0] = pSsaoBlurredBuffer->resourceView;
		rDrawState.psResources[1] = pAlbedoBuffer->resourceView;
		rDrawState.psResourceCount = 2;

		rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		rDrawState.psSamplerCount = 1;

		rDrawState.psConstantBuffers[0] = m_ssaoConstants;
		rDrawState.psConstantBufferCount = 1;

		pRdrContext->Draw(rDrawState, 1);
	}

	rDrawState.Reset();

	pRdrContext->EndEvent();
}

void RdrPostProcess::CopyToTarget(RdrContext* pRdrContext, RdrDrawState& rDrawState, 
	RdrResourceHandle hTextureInput, RdrRenderTargetViewHandle hTarget)
{
	RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(hTarget);
	RdrDepthStencilView depthView = { 0 };
	Vec2 viewportSize = g_pRenderer->GetViewportSize();
	pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
	pRdrContext->SetViewport(Rect(0.f, 0.f, viewportSize.x, viewportSize.y));
	pRdrContext->SetBlendState(RdrBlendMode::kOpaque);

	setupFullscreenDrawState(rDrawState);

	// Pixel shader
	const RdrResource* pCopyTexture = RdrResourceSystem::GetResource(hTextureInput);
	rDrawState.pPixelShader = RdrShaderSystem::GetPixelShader(m_hCopyPixelShader);
	rDrawState.psResources[0] = pCopyTexture->resourceView;
	rDrawState.psResourceCount = 1;
	rDrawState.psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
	rDrawState.psSamplerCount = 1;
	rDrawState.psConstantBufferCount = 0;

	pRdrContext->Draw(rDrawState, 1);
}