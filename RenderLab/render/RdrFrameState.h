#pragma once

#include <vector>
#include "RdrDeviceTypes.h"
#include "RdrResource.h"
#include "Camera.h"

struct RdrDrawOp;
class LightList;

#define MAX_ACTIONS_PER_FRAME 16
#define MAX_RENDER_TARGETS 6

struct RdrPass
{
	RdrRenderTargetView aRenderTargets[MAX_RENDER_TARGETS];
	RdrDepthStencilView depthTarget;

	// If no camera is set, an ortho projection is used.
	const Camera* pCamera;
	Rect viewport;

	RdrShaderMode shaderMode;
	RdrDepthTestMode depthTestMode;
	bool bAlphaBlend;

	bool bEnabled;
	bool bClearRenderTargets;
	bool bClearDepthTarget;
};

struct RdrAction
{
	void Reset();

	///
	LPCWSTR name;

	std::vector<RdrDrawOp*> buckets[kRdrBucketType_Count];
	RdrPass passes[kRdrPass_Count];
	bool bDoLightCulling;
	bool bIsCubemapCapture;

	Camera camera;
	const LightList* pLights;
};

struct RdrConstantBufferUpdate
{
	RdrResourceHandle hBuffer;
	Vec4 data[8];
};

struct RdrResourceUpdate
{
	RdrResourceHandle hResource;
	void* pData;
	uint dataSize;
};

struct RdrFrameState
{
	std::vector<RdrConstantBufferUpdate> constantBufferUpdates;
	std::vector<RdrResourceUpdate>       resourceUpdates;
	RdrAction                            actions[MAX_ACTIONS_PER_FRAME];
	uint                                 numActions;
};