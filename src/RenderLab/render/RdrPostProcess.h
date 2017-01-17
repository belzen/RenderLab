#pragma once

#include "AssetLib/AssetLibForwardDecl.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
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
	void Init();

	void QueueDraw(const AssetLib::PostProcessEffects& rEffects);

	void HandleResize(uint width, uint height);
	void DoPostProcessing(const InputManager& rInputManager, RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const AssetLib::PostProcessEffects& rEffects);

	const RdrPostProcessDbgData& GetDebugData() const;

private:
	void DoLuminanceMeasurement(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const RdrConstantBufferHandle hToneMapInputConstants);
	void DoLuminanceHistogram(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);
	void DoBloom(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const RdrConstantBufferHandle hToneMapInputConstants);
	void DoTonemap(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const RdrResource* pBloomBuffer);

	RdrConstantBufferHandle m_hToneMapInputConstants;

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
	// Debug
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