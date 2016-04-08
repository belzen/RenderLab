#pragma once

#include <vector>
#include "RdrDeviceTypes.h"
#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "Camera.h"

struct RdrDrawOp;
class LightList;

#define MAX_ACTIONS_PER_FRAME 16
#define MAX_RENDER_TARGETS 6

struct RdrPass
{
	RdrRenderTargetViewHandle ahRenderTargets[MAX_RENDER_TARGETS];
	RdrDepthStencilViewHandle hDepthTarget;

	Rect viewport;

	RdrShaderMode shaderMode;
	RdrDepthTestMode depthTestMode;
	bool bAlphaBlend;

	bool bEnabled;
	bool bClearRenderTargets;
	bool bClearDepthTarget;
};

struct RdrLightParams
{
	RdrResourceHandle hLightListRes;
	RdrResourceHandle hShadowMapDataRes;
	RdrResourceHandle hShadowMapTexArray;
	RdrResourceHandle hShadowCubeMapTexArray;
	uint lightCount;
};

struct RdrAction
{
	void Reset();

	///
	LPCWSTR name;

	std::vector<RdrDrawOp*> buckets[kRdrBucketType_Count];
	RdrPass passes[kRdrPass_Count];

	Camera camera;

	RdrLightParams lightParams;
	RdrConstantBufferHandle hPerActionVs;
	RdrConstantBufferHandle hPerActionPs;
	RdrConstantBufferHandle hPerActionGs;

	RdrConstantBufferHandle hUiPerActionVs;
	RdrConstantBufferHandle hUiPerActionPs;

	bool bIsCubemapCapture;
};

struct RdrFrameState
{
	RdrAction actions[MAX_ACTIONS_PER_FRAME];
	uint      numActions;
};