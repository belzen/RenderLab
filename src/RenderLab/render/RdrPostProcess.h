#pragma once

#include "AssetLib/AssetLibForwardDecl.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "RdrAction.h"
#include "debug/PostProcessDebugger.h"

class RdrContext;
class RdrDrawState;
struct RdrResource;
class InputManager;

struct RdrPostProcessDbgData
{
	Vec4 lumColor;
	float linearExposure;
	float adaptedLum;
	float lumAtCursor;
};

class RdrPostProcess
{
public:
	void Init(RdrContext* pRdrContext, const InputManager* pInputManager);

	void ResizeResources(uint width, uint height);
	void DoPostProcessing(RdrContext* pRdrContext, RdrDrawState* pDrawState,
		const RdrActionSurfaces& rBuffers, const AssetLib::PostProcessEffects& rEffects, const RdrGlobalConstants& rGlobalConstants);

	const RdrPostProcessDbgData& GetDebugData() const;

	RdrResourceHandle GetSsaoBlurredBuffer() const;

	void CopyToTarget(RdrContext* pRdrContext, RdrDrawState* pDrawState, RdrResourceHandle hTextureInput, RdrRenderTargetViewHandle hTarget);

private:
	void DoLuminanceMeasurement(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrResource* pColorBuffer);
	void DoLuminanceHistogram(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrResource* pColorBuffer);
	void DoBloom(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrResource* pColorBuffer);
	void DoTonemap(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrResource* pColorBuffer, const RdrResource* pBloomBuffer);

	void ResizeSsaoResources(uint width, uint height);
	void DoSsao(RdrContext* pRdrContext, RdrDrawState* pDrawState, const RdrActionSurfaces& rBuffers,
		const AssetLib::PostProcessEffects& rEffects, const RdrGlobalConstants& rGlobalConstants);

	RdrResource m_toneMapInputConstants;

	const RdrPipelineState* m_toneMapPipelineState;
	RdrResourceHandle m_hToneMapOutputConstants;

	const RdrPipelineState* m_toneMapHistogramPipelineState;
	RdrResourceHandle m_hToneMapTileHistograms;
	RdrResourceHandle m_hToneMapMergedHistogram;
	RdrResourceHandle m_hToneMapHistogramResponseCurve;
	RdrConstantBufferHandle m_hToneMapHistogramSettings;

	RdrResourceHandle m_hLumOutputs[4];

	struct BloomBuffer
	{
		RdrResourceHandle hResources[2];
		RdrConstantBufferHandle hBlendConstants;
	};
	BloomBuffer m_bloomBuffers[6];

	//////////////////////////////////////////////////////////////////////////
	// SSAO
	SsaoParams m_ssaoParams;
	RdrResource m_ssaoConstants;
	RdrResourceHandle m_hSsaoNoiseTexture;
	RdrRenderTarget m_ssaoBuffer;
	RdrRenderTarget m_ssaoBlurredBuffer;
	const RdrPipelineState* m_ssaoGenPipelineState;
	const RdrPipelineState* m_ssaoBlurPipelineState;
	const RdrPipelineState* m_ssaoApplyPipelineState;

	//////////////////////////////////////////////////////////////////////////
	// Debug
	const InputManager* m_pInputManager;
	const RdrPipelineState* m_copyPipelineState;
	PostProcessDebugger m_debugger;
	RdrPostProcessDbgData m_debugData;
	RdrResource m_lumDebugRes[3];
	RdrResource m_tonemapDebugRes[3];
	int m_dbgFrame;
	bool m_useHistogramToneMap;
};

inline const RdrPostProcessDbgData& RdrPostProcess::GetDebugData() const
{
	return m_debugData;
}

inline RdrResourceHandle RdrPostProcess::GetSsaoBlurredBuffer() const
{
	return m_ssaoBlurredBuffer.hTexture;
}
