#pragma once

#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "debug/PostProcessDebugger.h"

class RdrContext;
class RdrDrawState;
struct RdrResource;

struct RdrPostProcessDbgData
{
	Vec4 lumColor;
	float linearExposure;
	float avgLum;
	float lumAtCursor;
};

class RdrPostProcess
{
public:
	void Init();

	void HandleResize(uint width, uint height);
	void DoPostProcessing(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer);

	const RdrPostProcessDbgData& GetDebugData() const;

private:
	RdrShaderHandle m_hToneMapPs;
	RdrResourceHandle m_hToneMapOutputConstants;
	RdrConstantBufferHandle m_hToneMapInputConstants;

	RdrResourceHandle m_hLumOutputs[4];

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