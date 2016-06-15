#pragma once

#include "RdrDeviceTypes.h"
#include "RdrResource.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "Camera.h"
#include "Light.h"

struct RdrDrawOp;
class Scene;
class LightList;
class RdrPostProcessEffects;

typedef std::vector<const RdrDrawOp*> RdrDrawOpBucket;

#define MAX_ACTIONS_PER_FRAME 16
#define MAX_RENDER_TARGETS 6

struct RdrGlobalConstants
{
	RdrConstantBufferHandle hVsPerFrame;
	RdrConstantBufferHandle hPsPerFrame;
	RdrConstantBufferHandle hGsCubeMap;
};

struct RdrPassData
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
	bool bIsCubeMapCapture;
};

struct RdrShadowPass
{
	RdrPassData               passData;
	RdrGlobalConstants        constants;
	RdrDrawOpBucket           drawOps;
	Camera                    camera;
	RdrDepthStencilViewHandle hDepthView;
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

	RdrDrawOpBucket buckets[(int)RdrBucketType::Count];
	RdrPassData passes[(int)RdrPass::Count];

	RdrShadowPass shadowPasses[MAX_SHADOW_MAPS_PER_FRAME];
	int shadowPassCount;

	const Scene* pScene;
	Camera camera;

	Rect primaryViewport;

	const RdrPostProcessEffects* pPostProcEffects;

	RdrLightParams lightParams;
	RdrGlobalConstants constants;
	RdrGlobalConstants uiConstants;

	bool bIsCubemapCapture;
	bool bEnablePostProcessing;
};

struct RdrFrameState
{
	RdrAction actions[MAX_ACTIONS_PER_FRAME];
	uint      numActions;
};