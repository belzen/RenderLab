#pragma once

#include "RdrGeometry.h"
#include "RdrShaders.h"

class RdrContext;
class RdrDrawState;
struct RdrResource;
struct RdrAssetSystems;

class RdrPostProcess
{
public:
	void Init(RdrAssetSystems& rAssets);

	void HandleResize(uint width, uint height, RdrAssetSystems& rAssets);
	void DoPostProcessing(RdrContext* pRdrContext, RdrDrawState& rDrawState, const RdrResource* pColorBuffer, RdrAssetSystems& rAssets);

private:
	RdrShaderHandle m_hToneMapPs;
	RdrResourceHandle m_hToneMapConstants;

	RdrResourceHandle m_hLumOutputs[4];
};