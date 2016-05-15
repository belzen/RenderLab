#pragma once

#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "debug/PostProcessDebugger.h"

class RdrContext;
class RdrDrawState;
struct RdrResource;
namespace AssetLib
{
	struct PostProcessEffects;
}

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
	void DoPostProcessing(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, const AssetLib::PostProcessEffects& rEffects);

	const RdrPostProcessDbgData& GetDebugData() const;

private:
	void DoLuminanceMeasurement(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);
	void DoBloom(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);
	void DoTonemap(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);

	RdrShaderHandle m_hToneMapPs;
	RdrResourceHandle m_hToneMapOutputConstants;
	RdrConstantBufferHandle m_hToneMapInputConstants;

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
};

inline const RdrPostProcessDbgData& RdrPostProcess::GetDebugData() const
{
	return m_debugData;
}