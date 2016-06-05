#pragma once

#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "debug/PostProcessDebugger.h"

class RdrContext;
class RdrDrawState;
class RdrPostProcessEffects;
struct RdrResource;

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

	void HandleResize(uint width, uint height);
	void DoPostProcessing(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const RdrPostProcessEffects& rEffects);

	const RdrPostProcessDbgData& GetDebugData() const;

private:
	void DoLuminanceMeasurement(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const RdrConstantBufferHandle hToneMapInputConstants);
	void DoLuminanceHistogram(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);
	void DoBloom(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const RdrConstantBufferHandle hToneMapInputConstants);
	void DoTonemap(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);

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
		RdrConstantBufferHandle hAddConstants;
	};
	BloomBuffer m_bloomBuffers[4];

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