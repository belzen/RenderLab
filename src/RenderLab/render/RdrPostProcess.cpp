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
		if (!rLumCopyRes.IsValid())
		{
			RdrTextureInfo info = {};
			info.width = 1;
			info.height = 1;
			info.format = pColorBuffer->GetTextureInfo().format;
			info.depth = 1;
			info.mipLevels = 1;
			info.sampleCount = 1;
			pRdrContext->CreateTexture(nullptr, info, RdrResourceAccessFlags::CpuRO_GpuRW, rLumCopyRes);
		}
		pRdrContext->CopyResourceRegion(*pColorBuffer, srcRect, rLumCopyRes, IVec3::kZero);

		if (rLumReadRes.IsValid())
		{
			const uint pixelByteSize = 8;
			char dataBuffer[32];
			void* pAlignedData = dataBuffer;
			size_t space;
			std::align(16, pixelByteSize, pAlignedData, space);

			uint size = rdrGetTexturePitch(1, pColorBuffer->GetTextureInfo().format);
			pRdrContext->ReadResource(rLumReadRes, pAlignedData, pixelByteSize);

			rOutData.lumColor = Maths::convertHalfToSinglePrecision4((float16*)pAlignedData);
			rOutData.lumAtCursor = calcLuminance(rOutData.lumColor);
		}
	}

	void dbgTonemapOutput(RdrContext* pRdrContext, const RdrResource* pTonemapOutputBuffer, RdrResource& rTonemapCopyRes, RdrResource& rTonemapReadRes, RdrPostProcessDbgData& rOutData)
	{
		RdrBox srcRect(0, 0, 0, sizeof(ToneMapOutputParams), 1, 1);
		if (!rTonemapCopyRes.IsValid())
		{
			pRdrContext->CreateStructuredBuffer(nullptr, 1, srcRect.width, RdrResourceAccessFlags::CpuRO_GpuRW, rTonemapCopyRes);
		}
		pRdrContext->CopyResourceRegion(*pTonemapOutputBuffer, srcRect, rTonemapCopyRes, IVec3::kZero);

		if (rTonemapReadRes.IsValid())
		{
			ToneMapOutputParams tonemap;
			pRdrContext->ReadResource(rTonemapReadRes, &tonemap, sizeof(tonemap));
			rOutData.linearExposure = tonemap.linearExposure;
			rOutData.adaptedLum = tonemap.adaptedLum;
		}
	}

	void setupFullscreenDrawState(RdrDrawState* pDrawState)
	{
		pDrawState->pVertexBuffers[0] = nullptr;
		pDrawState->vertexStrides[0] = 0;
		pDrawState->vertexOffsets[0] = 0;
		pDrawState->vertexBufferCount = 0;
		pDrawState->vertexCount = 3;
		pDrawState->eTopology = RdrTopology::TriangleList;
	}

	RdrPipelineState createFullscreenPipelineState(RdrContext* pRdrContext, const RdrShader* pPixelShader,
		const RdrResourceFormat* pRtvFormats, uint nNumRtvFormats,
		RdrBlendMode eBlendMode)
	{
		const RdrShader* pVertexShader = RdrShaderSystem::GetVertexShader(kScreenVertexShader);

		RdrRasterState rasterState;
		rasterState.bWireframe = false;
		rasterState.bDoubleSided = false;
		rasterState.bEnableMSAA = false;
		rasterState.bUseSlopeScaledDepthBias = false;
		rasterState.bEnableScissor = false;

		return pRdrContext->CreateGraphicsPipelineState(
			pVertexShader, pPixelShader,
			nullptr, nullptr,
			nullptr, 0,
			pRtvFormats, nNumRtvFormats,
			eBlendMode,
			rasterState,
			RdrDepthStencilState(false, false, RdrComparisonFunc::Never));
	}
}

void RdrPostProcess::Init(RdrContext* pRdrContext, const InputManager* pInputManager)
{
	m_pInputManager = pInputManager;
	m_debugger.Init(this);
	m_useHistogramToneMap = false;

	const RdrResourceFormat* pSceneRtvFormats;
	const RdrResourceFormat* pPrimaryRtvFormats;
	uint nNumSceneRtvFormats, nNumPrimaryRtvFormats;
	Renderer::GetStageRenderTargetFormats(RdrRenderStage::kScene, &pSceneRtvFormats, &nNumSceneRtvFormats);
	Renderer::GetStageRenderTargetFormats(RdrRenderStage::kPrimary, &pPrimaryRtvFormats, &nNumPrimaryRtvFormats);

	const RdrShader* pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_tonemap.hlsl", nullptr, 0);
	m_toneMapPipelineState = createFullscreenPipelineState(pRdrContext, pPixelShader, pPrimaryRtvFormats, nNumPrimaryRtvFormats, RdrBlendMode::kOpaque);
	m_hToneMapOutputConstants = g_pRenderer->GetResourceCommandList().CreateStructuredBuffer(nullptr
		, 1, sizeof(ToneMapOutputParams)
		, RdrResourceAccessFlags::GpuRW
		, CREATE_BACKPOINTER(this));

	const char* histogramDefines[] = { "TONEMAP_HISTOGRAM" };
	pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_tonemap.hlsl", histogramDefines, 1);
	m_toneMapHistogramPipelineState = createFullscreenPipelineState(pRdrContext, pPixelShader, pPrimaryRtvFormats, nNumPrimaryRtvFormats, RdrBlendMode::kOpaque);

	pRdrContext->CreateConstantBuffer(nullptr, sizeof(ToneMapInputParams), RdrResourceAccessFlags::CpuRW_GpuRO, m_toneMapInputConstants);

	pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_copy.hlsl", nullptr, 0);
	m_copyPipelineState = createFullscreenPipelineState(pRdrContext, pPixelShader, pSceneRtvFormats, nNumSceneRtvFormats, RdrBlendMode::kOpaque);

	const RdrResourceFormat rtvFormat = RdrResourceFormat::R8_UNORM;
	pRdrContext->CreateConstantBuffer(nullptr, sizeof(SsaoParams), RdrResourceAccessFlags::CpuRW_GpuRO, m_ssaoConstants);
	pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_ssao_gen.hlsl", nullptr, 0);
	m_ssaoGenPipelineState = createFullscreenPipelineState(pRdrContext, pPixelShader, &rtvFormat, 1, RdrBlendMode::kOpaque);

	pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_ssao_blur.hlsl", nullptr, 0);
	m_ssaoBlurPipelineState = createFullscreenPipelineState(pRdrContext, pPixelShader, &rtvFormat, 1, RdrBlendMode::kOpaque);

	pPixelShader = RdrShaderSystem::CreatePixelShaderFromFile("p_ssao_apply.hlsl", nullptr, 0);
	m_ssaoApplyPipelineState = createFullscreenPipelineState(pRdrContext, pPixelShader, pSceneRtvFormats, nNumSceneRtvFormats, RdrBlendMode::kSubtractive);
}

void RdrPostProcess::ResizeResources(uint width, uint height)
{
	RdrResourceCommandList& rResCommandList = g_pRenderer->GetResourceCommandList();

	// Update luminance measure textures
	uint i = 0;
	uint w = width;
	uint h = height;
	while (w > 16 || h > 16)
	{
		if (m_hLumOutputs[i])
			rResCommandList.ReleaseResource(m_hLumOutputs[i], CREATE_BACKPOINTER(this));

		w = (uint)ceil(w / 16.f);
		h = (uint)ceil(h / 16.f);

		m_hLumOutputs[i] = rResCommandList.CreateTexture2D(w, h, RdrResourceFormat::R16G16B16A16_FLOAT, 
			RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
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
				rResCommandList.ReleaseResource(rBloom.hResources[n], CREATE_BACKPOINTER(this));
			rBloom.hResources[n] = rResCommandList.CreateTexture2D(w, h, RdrResourceFormat::R16G16B16A16_FLOAT, 
				RdrResourceAccessFlags::GpuRW, nullptr, CREATE_BACKPOINTER(this));
		}

		if (rBloom.hBlendConstants)
			rResCommandList.ReleaseConstantBuffer(rBloom.hBlendConstants, CREATE_BACKPOINTER(this));

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

		rBloom.hBlendConstants = rResCommandList.CreateConstantBuffer(pBlendParams, constantsSize, RdrResourceAccessFlags::CpuRO_GpuRO, CREATE_BACKPOINTER(this));
	}

	if (m_useHistogramToneMap)
	{
		uint numTiles = (uint)ceilf(width / 32.f) * (uint)ceilf(height / 16.f); // todo - share tile sizes & histogram bin sizes with shader
		if (m_hToneMapTileHistograms)
			rResCommandList.ReleaseResource(m_hToneMapTileHistograms, CREATE_BACKPOINTER(this));
		m_hToneMapTileHistograms = rResCommandList.CreateDataBuffer(nullptr, numTiles * 64, RdrResourceFormat::R16_UINT, RdrResourceAccessFlags::GpuRW, CREATE_BACKPOINTER(this));
	
		if (m_hToneMapMergedHistogram)
			rResCommandList.ReleaseResource(m_hToneMapMergedHistogram, CREATE_BACKPOINTER(this));
		m_hToneMapMergedHistogram = rResCommandList.CreateDataBuffer(nullptr, 64, RdrResourceFormat::R32_UINT, RdrResourceAccessFlags::GpuRW, CREATE_BACKPOINTER(this));

		if (m_hToneMapHistogramResponseCurve)
			rResCommandList.ReleaseResource(m_hToneMapHistogramResponseCurve, CREATE_BACKPOINTER(this));
		m_hToneMapHistogramResponseCurve = rResCommandList.CreateDataBuffer(nullptr, 64, RdrResourceFormat::R16_FLOAT, RdrResourceAccessFlags::GpuRW, CREATE_BACKPOINTER(this));


		if (m_hToneMapHistogramSettings)
			rResCommandList.ReleaseConstantBuffer(m_hToneMapHistogramSettings, CREATE_BACKPOINTER(this));

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
		m_hToneMapHistogramSettings = rResCommandList.CreateConstantBuffer(pTest, constantsSize, RdrResourceAccessFlags::CpuRO_GpuRO, CREATE_BACKPOINTER(this));
	}

	ResizeSsaoResources(width, height);
}

void RdrPostProcess::DoPostProcessing(RdrContext* pRdrContext, RdrDrawState* pDrawState, 
	const RdrActionSurfaces& rBuffers, const AssetLib::PostProcessEffects& rEffects, const RdrGlobalConstants& rGlobalConstants)
{
	const RdrResource* pColorBuffer = RdrResourceSystem::GetResource(rBuffers.colorBuffer.hTexture);

	pRdrContext->BeginEvent(L"Post-Process");

	if (rEffects.ssao.enabled)
	{
		DoSsao(pRdrContext, pDrawState, rBuffers, rEffects, rGlobalConstants);
	}

	m_dbgFrame = (m_dbgFrame + 1) % 3;

	if (m_debugger.IsActive() && m_pInputManager)
	{
		int dbgReadIdx = getDbgReadIndex(m_dbgFrame);
		dbgLuminanceInput(*m_pInputManager, pRdrContext, pColorBuffer, m_lumDebugRes[m_dbgFrame], m_lumDebugRes[dbgReadIdx], m_debugData);
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
	pRdrContext->UpdateResource(m_toneMapInputConstants, &tonemapSettings, sizeof(ToneMapInputParams));

	//////////////////////////////////////////////////////////////////////////
	RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(RdrGlobalRenderTargetHandles::kPrimary);
	RdrDepthStencilView depthView = pRdrContext->GetNullDepthStencilView();
	pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
	pRdrContext->SetViewport(Rect(0.f, 0.f, (float)pColorBuffer->GetTextureInfo().width, (float)pColorBuffer->GetTextureInfo().height));

	//////////////////////////////////////////////////////////////////////////
	if (m_useHistogramToneMap)
	{
		DoLuminanceHistogram(pRdrContext, pDrawState, pColorBuffer);
	}
	else
	{
		DoLuminanceMeasurement(pRdrContext, pDrawState, pColorBuffer);
	}

	const RdrResource* pBloomBuffer = nullptr;
	if (rEffects.bloom.enabled)
	{
		DoBloom(pRdrContext, pDrawState, pColorBuffer);
		pBloomBuffer = RdrResourceSystem::GetResource(m_bloomBuffers[0].hResources[1]);
	}
	else
	{
		pBloomBuffer = RdrResourceSystem::GetDefaultResource(RdrDefaultResource::kBlackTex2d);
	}

	DoTonemap(pRdrContext, pDrawState, pColorBuffer, pBloomBuffer);

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoLuminanceMeasurement(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Lum Measurement");
	
	uint w = RdrComputeOp::getThreadGroupCount(pColorBuffer->GetTextureInfo().width, 16);
	uint h = RdrComputeOp::getThreadGroupCount(pColorBuffer->GetTextureInfo().height, 16);

	const RdrResource* pLumInput = pColorBuffer;
	const RdrResource* pLumOutput = RdrResourceSystem::GetResource(m_hLumOutputs[0]);
	const RdrResource* pTonemapOutput = RdrResourceSystem::GetResource(m_hToneMapOutputConstants);

	pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::LuminanceMeasure_First);
	pDrawState->csResources[0] = pLumInput->GetSRV();
	pDrawState->csResourceCount = 1;
	pDrawState->csUavs[0] = pLumOutput->GetUAV();
	pDrawState->csUavCount = 1;
	pDrawState->csConstantBuffers[0] = m_toneMapInputConstants.GetSRV();
	pDrawState->csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(*pDrawState, w, h, 1);

	uint i = 1;
	pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::LuminanceMeasure_Mid);
	while (w > 16 || h > 16)
	{
		pLumInput = pLumOutput;
		pLumOutput = RdrResourceSystem::GetResource(m_hLumOutputs[i]);

		pDrawState->csResources[0] = pLumInput->GetSRV();
		pDrawState->csResourceCount = 1;
		pDrawState->csUavs[0] = pLumOutput->GetUAV();
		pDrawState->csUavCount = 1;

		w = RdrComputeOp::getThreadGroupCount(w, 16);
		h = RdrComputeOp::getThreadGroupCount(h, 16);
		++i;

		pRdrContext->DispatchCompute(*pDrawState, w, h, 1);
	}

	pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::LuminanceMeasure_Final);
	pDrawState->csResources[0] = pLumOutput->GetSRV();
	pDrawState->csResourceCount = 1;
	pDrawState->csUavs[0] = pTonemapOutput->GetUAV();
	pDrawState->csUavCount = 1;
	pRdrContext->DispatchCompute(*pDrawState, 1, 1, 1);

	pDrawState->Reset();

	if (m_debugger.IsActive())
	{
		int dbgReadIdx = getDbgReadIndex(m_dbgFrame);
		dbgTonemapOutput(pRdrContext, pTonemapOutput, m_tonemapDebugRes[m_dbgFrame], m_tonemapDebugRes[dbgReadIdx], m_debugData);
	}

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoLuminanceHistogram(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Lum Histogram");

	uint w = RdrComputeOp::getThreadGroupCount(pColorBuffer->GetTextureInfo().width, 32);
	uint h = RdrComputeOp::getThreadGroupCount(pColorBuffer->GetTextureInfo().height, 16);

	const RdrResource* pLumInput = pColorBuffer;
	const RdrResource* pTileHistograms = RdrResourceSystem::GetResource(m_hToneMapTileHistograms);
	const RdrResource* pMergedHistogram = RdrResourceSystem::GetResource(m_hToneMapMergedHistogram);
	const RdrResource* pResponseCurve = RdrResourceSystem::GetResource(m_hToneMapHistogramResponseCurve);
	const RdrResource* pToneMapParams = RdrResourceSystem::GetConstantBuffer(m_hToneMapHistogramSettings);

	pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::LuminanceHistogram_Tile);
	pDrawState->csResources[0] = pColorBuffer->GetSRV();
	pDrawState->csResourceCount = 1;
	pDrawState->csUavs[0] = pTileHistograms->GetUAV();
	pDrawState->csUavCount = 1;
	pDrawState->csConstantBuffers[0] = pToneMapParams->GetSRV();
	pDrawState->csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(*pDrawState, w, h, 1);

	pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::LuminanceHistogram_Merge);
	pDrawState->csResources[0] = pTileHistograms->GetSRV();
	pDrawState->csResourceCount = 1;
	pDrawState->csUavs[0] = pMergedHistogram->GetUAV();
	pDrawState->csUavCount = 1;
	pDrawState->csConstantBuffers[0] = pToneMapParams->GetSRV();
	pDrawState->csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(*pDrawState, (uint)ceilf(2700.f / 1024.f), 1, 1); //todo thread counts

	pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::LuminanceHistogram_ResponseCurve);
	pDrawState->csResources[0] = pMergedHistogram->GetSRV();
	pDrawState->csResourceCount = 1;
	pDrawState->csUavs[0] = pResponseCurve->GetUAV();
	pDrawState->csUavCount = 1;
	pDrawState->csConstantBuffers[0] = pToneMapParams->GetSRV();
	pDrawState->csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(*pDrawState, 1, 1, 1);

	pDrawState->Reset();

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoBloom(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrResource* pColorBuffer)
{
	pRdrContext->BeginEvent(L"Bloom");

	// High-pass filter and downsample
	const RdrResource* pLumInput = pColorBuffer;
	const RdrResource* pTonemapBuffer = RdrResourceSystem::GetResource(m_hToneMapOutputConstants);
	const RdrResource* pHighPassShrinkOutput = RdrResourceSystem::GetResource(m_bloomBuffers[0].hResources[0]);

	uint w = RdrComputeOp::getThreadGroupCount(pHighPassShrinkOutput->GetTextureInfo().width, SHRINK_THREADS_X);
	uint h = RdrComputeOp::getThreadGroupCount(pHighPassShrinkOutput->GetTextureInfo().height, SHRINK_THREADS_Y);

	pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::BloomShrink);
	pDrawState->csResources[0] = pLumInput->GetSRV();
	pDrawState->csResources[1] = pTonemapBuffer->GetSRV();
	pDrawState->csResourceCount = 2;
	pDrawState->csUavs[0] = pHighPassShrinkOutput->GetUAV();
	pDrawState->csUavCount = 1;
	pDrawState->csConstantBuffers[0] = m_toneMapInputConstants.GetSRV();
	pDrawState->csConstantBufferCount = 1;
	pRdrContext->DispatchCompute(*pDrawState, w, h, 1);

	for (int i = 1; i < ARRAY_SIZE(m_bloomBuffers); ++i)
	{
		const RdrResource* pTexInput = RdrResourceSystem::GetResource(m_bloomBuffers[i - 1].hResources[0]);
		const RdrResource* pTexOutput = RdrResourceSystem::GetResource(m_bloomBuffers[i].hResources[0]);

		w = RdrComputeOp::getThreadGroupCount(pTexOutput->GetTextureInfo().width, SHRINK_THREADS_X);
		h = RdrComputeOp::getThreadGroupCount(pTexOutput->GetTextureInfo().height, SHRINK_THREADS_Y);

		pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::Shrink);
		pDrawState->csResources[0] = pTexInput->GetSRV();
		pDrawState->csResourceCount = 1;
		pDrawState->csUavs[0] = pTexOutput->GetUAV();
		pDrawState->csUavCount = 1;
		pDrawState->csConstantBufferCount = 0;
		pRdrContext->DispatchCompute(*pDrawState, w, h, 1);

		// Wait for UAV writes to complete
		pRdrContext->UAVBarrier(pTexOutput);
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

			uint w = RdrComputeOp::getThreadGroupCount(pOutput->GetTextureInfo().width, BLUR_THREAD_COUNT);
			uint h = RdrComputeOp::getThreadGroupCount(pOutput->GetTextureInfo().height, BLUR_TEXELS_PER_THREAD);

			pDrawState->Reset();
			pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::BlurVertical);
			pDrawState->csResources[0] = pInput1->GetSRV();
			pDrawState->csResourceCount = 1;
			pDrawState->csUavs[0] = pOutput->GetUAV();
			pDrawState->csUavCount = 1;
			pDrawState->csConstantBufferCount = 0;
			pRdrContext->DispatchCompute(*pDrawState, w, h, 1);
			pDrawState->Reset();

			// Wait for UAV writes
			pRdrContext->UAVBarrier(pOutput);

			// horizontal blur
			const RdrResource* pTemp = pInput1;
			pInput1 = pOutput;
			pOutput = pTemp;

			w = RdrComputeOp::getThreadGroupCount(pOutput->GetTextureInfo().width, BLUR_TEXELS_PER_THREAD);
			h = RdrComputeOp::getThreadGroupCount(pOutput->GetTextureInfo().height, BLUR_THREAD_COUNT);

			pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::BlurHorizontal);
			pDrawState->csResources[0] = pInput1->GetSRV();
			pDrawState->csResourceCount = 1;
			pDrawState->csUavs[0] = pOutput->GetUAV();
			pDrawState->csUavCount = 1;
			pDrawState->csConstantBufferCount = 0;
			pRdrContext->DispatchCompute(*pDrawState, w, h, 1);
			pDrawState->Reset();

			// Wait for UAV writes
			pRdrContext->UAVBarrier(pOutput);
		}

		// Blend together
		if (i != 0)
		{
			pInput1 = RdrResourceSystem::GetResource(m_bloomBuffers[i - 1].hResources[0]);
			pInput2 = pOutput;
			pOutput = RdrResourceSystem::GetResource(m_bloomBuffers[i - 1].hResources[1]);

			uint w = RdrComputeOp::getThreadGroupCount(pOutput->GetTextureInfo().width, BLEND_THREADS_X);
			uint h = RdrComputeOp::getThreadGroupCount(pOutput->GetTextureInfo().height, BLEND_THREADS_Y);

			pDrawState->pipelineState = RdrShaderSystem::GetComputeShaderPipelineState(RdrComputeShader::Blend2d);
			pDrawState->csResources[0] = pInput1->GetSRV();
			pDrawState->csResources[1] = pInput2->GetSRV();
			pDrawState->csResourceCount = 2;
			pDrawState->csSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
			pDrawState->csSamplerCount = 1;
			pDrawState->csUavs[0] = pOutput->GetUAV();
			pDrawState->csUavCount = 1;
			pDrawState->csConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(m_bloomBuffers[i - 1].hBlendConstants)->GetSRV();
			pDrawState->csConstantBufferCount = 1;
			pRdrContext->DispatchCompute(*pDrawState, w, h, 1);
			pDrawState->Reset();

			// Wait for UAV writes
			pRdrContext->UAVBarrier(pOutput);
		}
	}

	pRdrContext->EndEvent();
}

void RdrPostProcess::DoTonemap(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrResource* pColorBuffer, const RdrResource* pBloomBuffer)
{
	pRdrContext->BeginEvent(L"Tonemap");
	setupFullscreenDrawState(pDrawState);

	// Pixel shader
	if (m_useHistogramToneMap)
	{
		pDrawState->pipelineState = m_toneMapHistogramPipelineState;

		pDrawState->psResources[0] = pColorBuffer->GetSRV();
		pDrawState->psResources[1] = pBloomBuffer ? pBloomBuffer->GetSRV() : RdrShaderResourceView();
		pDrawState->psResources[2] = RdrResourceSystem::GetResource(m_hToneMapOutputConstants)->GetSRV();
		pDrawState->psResources[3] = RdrResourceSystem::GetResource(m_hToneMapHistogramResponseCurve)->GetSRV();
		pDrawState->psResourceCount = 4;

		pDrawState->psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		pDrawState->psSamplerCount = 1;

		pDrawState->psConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(m_hToneMapHistogramSettings)->GetSRV();
		pDrawState->psConstantBufferCount = 1;
	}
	else
	{
		pDrawState->pipelineState = m_toneMapPipelineState;

		pDrawState->psResources[0] = pColorBuffer->GetSRV();
		pDrawState->psResources[1] = pBloomBuffer ? pBloomBuffer->GetSRV() : RdrShaderResourceView();
		pDrawState->psResources[2] = RdrResourceSystem::GetResource(m_hToneMapOutputConstants)->GetSRV();
		pDrawState->psResourceCount = 3;

		pDrawState->psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		pDrawState->psSamplerCount = 1;
	}

	pRdrContext->Draw(*pDrawState, 1);
	pRdrContext->EndEvent();

	pDrawState->Reset();
}


void RdrPostProcess::ResizeSsaoResources(uint width, uint height)
{
	//http://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html
	RdrResourceCommandList& rResCommands = g_pRenderer->GetResourceCommandList();
	const UVec2 ssaoBufferSize = UVec2(width / 2, height / 2);

	if (m_hSsaoNoiseTexture)
		rResCommands.ReleaseResource(m_hSsaoNoiseTexture, CREATE_BACKPOINTER(this));

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
		RdrResourceAccessFlags::CpuRO_GpuRO, pNoiseTexData, CREATE_BACKPOINTER(this));

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
	rResCommands.ReleaseRenderTarget2d(m_ssaoBuffer, CREATE_BACKPOINTER(this));
	m_ssaoBuffer = rResCommands.InitRenderTarget2d(ssaoBufferSize.x, ssaoBufferSize.y, RdrResourceFormat::R8_UNORM, 1, CREATE_BACKPOINTER(this));

	rResCommands.ReleaseRenderTarget2d(m_ssaoBlurredBuffer, CREATE_BACKPOINTER(this));
	m_ssaoBlurredBuffer = rResCommands.InitRenderTarget2d(ssaoBufferSize.x, ssaoBufferSize.y, RdrResourceFormat::R8_UNORM, 1, CREATE_BACKPOINTER(this));
}

void RdrPostProcess::DoSsao(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrActionSurfaces& rBuffers,
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
	pRdrContext->UpdateResource(m_ssaoConstants, &m_ssaoParams, sizeof(m_ssaoParams));

	// Generate SSAO buffer
	{
		RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(m_ssaoBuffer.hRenderTarget);
		RdrDepthStencilView depthView = pRdrContext->GetNullDepthStencilView();
		pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
		pRdrContext->SetViewport(Rect(0.f, 0.f, (float)pSsaoBuffer->GetTextureInfo().width, (float)pSsaoBuffer->GetTextureInfo().height));

		setupFullscreenDrawState(pDrawState);
		pDrawState->pipelineState = m_ssaoGenPipelineState;

		// Pixel shader
		pDrawState->psResources[0] = pDepthBuffer->GetSRV();
		pDrawState->psResources[1] = pNormalBuffer->GetSRV();
		pDrawState->psResources[2] = pNoiseBuffer->GetSRV();
		pDrawState->psResourceCount = 3;

		pDrawState->psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		pDrawState->psSamplers[1] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
		pDrawState->psSamplerCount = 2;

		pDrawState->psConstantBuffers[0] = RdrResourceSystem::GetConstantBuffer(rGlobalConstants.hPsPerAction)->GetSRV();
		pDrawState->psConstantBuffers[1] = m_ssaoConstants.GetSRV();
		pDrawState->psConstantBufferCount = 2;

		pRdrContext->Draw(*pDrawState, 1);
	}

	// Blur
	{
		RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(m_ssaoBlurredBuffer.hRenderTarget);
		RdrDepthStencilView depthView = pRdrContext->GetNullDepthStencilView();
		Vec2 viewportSize = g_pRenderer->GetViewportSize();
		pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
		pRdrContext->SetViewport(Rect(0.f, 0.f, (float)pSsaoBlurredBuffer->GetTextureInfo().width, (float)pSsaoBlurredBuffer->GetTextureInfo().height));

		setupFullscreenDrawState(pDrawState);
		pDrawState->pipelineState = m_ssaoBlurPipelineState;

		// Pixel shader
		pDrawState->psResources[0] = pSsaoBuffer->GetSRV();
		pDrawState->psResourceCount = 1;

		pDrawState->psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		pDrawState->psSamplerCount = 1;

		pDrawState->psConstantBuffers[0] = m_ssaoConstants.GetSRV();
		pDrawState->psConstantBufferCount = 1;

		pRdrContext->Draw(*pDrawState, 1);
	}

	// Apply ambient occlusion
	{
		RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(rBuffers.colorBuffer.hRenderTarget);
		RdrDepthStencilView depthView = pRdrContext->GetNullDepthStencilView();
		Vec2 viewportSize = g_pRenderer->GetViewportSize();
		pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
		pRdrContext->SetViewport(Rect(0.f, 0.f, viewportSize.x, viewportSize.y));

		setupFullscreenDrawState(pDrawState);
		pDrawState->pipelineState = m_ssaoApplyPipelineState;

		// Pixel shader
		pDrawState->psResources[0] = pSsaoBlurredBuffer->GetSRV();
		pDrawState->psResources[1] = pAlbedoBuffer->GetSRV();
		pDrawState->psResourceCount = 2;

		pDrawState->psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
		pDrawState->psSamplerCount = 1;

		pDrawState->psConstantBuffers[0] = m_ssaoConstants.GetSRV();
		pDrawState->psConstantBufferCount = 1;

		pRdrContext->Draw(*pDrawState, 1);
	}

	pDrawState->Reset();

	pRdrContext->EndEvent();
}

void RdrPostProcess::CopyToTarget(RdrContext* pRdrContext, RdrDrawState* pDrawState,
	RdrResourceHandle hTextureInput, RdrRenderTargetViewHandle hTarget)
{
	RdrRenderTargetView renderTarget = RdrResourceSystem::GetRenderTargetView(hTarget);
	RdrDepthStencilView depthView = pRdrContext->GetNullDepthStencilView();
	Vec2 viewportSize = g_pRenderer->GetViewportSize();
	pRdrContext->SetRenderTargets(1, &renderTarget, depthView);
	pRdrContext->SetViewport(Rect(0.f, 0.f, viewportSize.x, viewportSize.y));

	setupFullscreenDrawState(pDrawState);
	pDrawState->pipelineState = m_copyPipelineState;

	// Pixel shader
	const RdrResource* pCopyTexture = RdrResourceSystem::GetResource(hTextureInput);
	pDrawState->psResources[0] = pCopyTexture->GetSRV();
	pDrawState->psResourceCount = 1;
	pDrawState->psSamplers[0] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Clamp, false);
	pDrawState->psSamplerCount = 1;
	pDrawState->psConstantBufferCount = 0;

	pRdrContext->Draw(*pDrawState, 1);
}