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
	void Init(RdrContext* pRdrContext);

	void HandleResize(uint width, uint height);
	void DoPostProcessing(const InputManager& rInputManager, RdrContext* pRdrContext, RdrDrawState& rDrawState, 
		const RdrActionSurfaces& rBuffers, const AssetLib::PostProcessEffects& rEffects, const RdrGlobalConstants& rGlobalConstants);

	const RdrPostProcessDbgData& GetDebugData() const;

	RdrResourceHandle GetSsaoBlurredBuffer() const;

	void CopyToTarget(RdrContext* pRdrContext, RdrDrawState& rDrawState, RdrResourceHandle hTextureInput, RdrRenderTargetViewHandle hTarget);

private:
	void DoLuminanceMeasurement(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);
	void DoLuminanceHistogram(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);
	void DoBloom(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);
	void DoTonemap(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const RdrResource* pBloomBuffer);

	void ResizeSsaoResources(uint width, uint height);
	void DoSsao(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrActionSurfaces& rBuffers,
		const AssetLib::PostProcessEffects& rEffects, const RdrGlobalConstants& rGlobalConstants);

	RdrConstantBufferDeviceObj m_toneMapInputConstants;

	RdrShaderHandle m_hToneMapPs;
	RdrResourceHandle m_hToneMapOutputConstants;

	RdrShaderHandle m_hToneMapHistogramPs;
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
	RdrConstantBufferDeviceObj m_ssaoConstants;
	RdrResourceHandle m_hSsaoNoiseTexture;
	RdrRenderTarget2d m_ssaoBuffer;
	RdrRenderTarget2d m_ssaoBlurredBuffer;
	RdrShaderHandle m_hSsaoGenPixelShader;
	RdrShaderHandle m_hSsaoBlurPixelShader;
	RdrShaderHandle m_hSsaoApplyPixelShader;

	//////////////////////////////////////////////////////////////////////////
	// Debug
	RdrShaderHandle m_hCopyPixelShader;

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
