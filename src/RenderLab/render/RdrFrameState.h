#pragma once

#include "RdrDeviceTypes.h"
#include "RdrResource.h"
#include "RdrResourceSystem.h"
#include "RdrShaders.h"
#include "RdrGeometry.h"
#include "RdrDrawOp.h"
#include "Camera.h"
#include "RdrLighting.h"
#include "shapes\Rect.h"
#include "AssetLib\SceneAsset.h"

struct RdrDrawOp;
struct RdrComputeOp;
class LightList;
class InputManager;

struct RdrDrawBucketEntry
{
	RdrDrawBucketEntry(const RdrDrawOp* pDrawOp) 
		: pDrawOp(pDrawOp)
	{
		// TODO: Provide actual depth value for alpha sorting.
		RdrDrawOp::BuildSortKey(pDrawOp, 0.f, sortKey);
	}

	static bool SortCompare(const RdrDrawBucketEntry& rLeft, const RdrDrawBucketEntry& rRight)
	{
		if (rLeft.sortKey.compare.val1 != rRight.sortKey.compare.val1)
			return rLeft.sortKey.compare.val1 < rRight.sortKey.compare.val1;
		else if (rLeft.sortKey.compare.val2 != rRight.sortKey.compare.val2)
			return rLeft.sortKey.compare.val2 < rRight.sortKey.compare.val2;
		return rLeft.pDrawOp < rRight.pDrawOp;
	}

	RdrDrawOpSortKey sortKey;
	const RdrDrawOp* pDrawOp;
};

typedef std::vector<RdrDrawBucketEntry> RdrDrawOpBucket;
typedef std::vector<const RdrComputeOp*> RdrComputeOpBucket;

#define MAX_ACTIONS_PER_FRAME 8
#define MAX_RENDER_TARGETS 6

struct RdrGlobalConstants
{
	RdrConstantBufferHandle hVsPerAction;
	RdrConstantBufferHandle hPsPerAction;
	RdrConstantBufferHandle hPsAtmosphere;
	RdrConstantBufferHandle hGsCubeMap;
};

struct RdrPassData
{
	RdrRenderTargetViewHandle ahRenderTargets[MAX_RENDER_TARGETS];
	RdrDepthStencilViewHandle hDepthTarget;

	Rect viewport;

	RdrShaderMode shaderMode;
	RdrDepthTestMode depthTestMode;
	bool bDepthWriteEnabled;
	bool bAlphaBlend;

	bool bEnabled;
	bool bClearRenderTargets;
	bool bClearDepthTarget;
	bool bIsCubeMapCapture;
};

class RdrDrawBuckets
{
public:
	void AddDrawOp(const RdrDrawOp* pDrawOp, RdrBucketType eBucket);
	void AddComputeOp(const RdrComputeOp* pComputeOp, RdrPass ePass);

	const RdrDrawOpBucket& GetDrawOpBucket(RdrBucketType eBucketType) const;
	const RdrComputeOpBucket& GetComputeOpBucket(RdrPass ePass) const;

	void SortDrawOps(RdrBucketType eBucketType);

	void Clear();

private:
	RdrDrawOpBucket m_drawOpBuckets[(int)RdrBucketType::Count];
	RdrComputeOpBucket m_computeOpBuckets[(int)RdrPass::Count];
};

struct RdrShadowPass
{
	RdrPassData               passData;
	RdrGlobalConstants        constants;
	RdrDrawBuckets            buckets;
	Camera                    camera;
	RdrDepthStencilViewHandle hDepthView;
};

struct RdrAction
{
	void Reset();

	///
	LPCWSTR name;

	RdrResourceCommandList resourceCommands;

	RdrDrawBuckets opBuckets;
	RdrPassData passes[(int)RdrPass::Count];

	RdrShadowPass shadowPasses[MAX_SHADOW_MAPS_PER_FRAME];
	int shadowPassCount;

	Camera camera;
	AssetLib::SkySettings sky;
	AssetLib::PostProcessEffects postProcessEffects;

	Rect primaryViewport;

	RdrLightResources lightParams;
	RdrLightList lights;

	RdrGlobalConstants constants;
	RdrGlobalConstants uiConstants;

	bool bIsCubemapCapture;
	bool bEnablePostProcessing;
};

struct RdrFrameState
{
	RdrAction actions[MAX_ACTIONS_PER_FRAME];
	uint      numActions;

	RdrResourceCommandList preFrameResourceCommands;
	RdrResourceCommandList postFrameResourceCommands;
};