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

struct RdrShadowPass
{
	RdrPassData               passData;
	RdrGlobalConstants        constants;
	Camera                    camera;
	RdrDepthStencilViewHandle hDepthView;
};

struct RdrSurface
{
	RdrRenderTargetViewHandle hRenderTarget;
	RdrDepthStencilViewHandle hDepthTarget;
};

class RdrAction
{
public:
	static RdrAction* CreatePrimary(Camera& rCamera);
	static RdrAction* CreateOffscreen(const wchar_t* actionName, Camera& rCamera,
		bool enablePostprocessing, const Rect& viewport, const RdrSurface& outputSurface);

public:
	void Release();

	void AddDrawOp(const RdrDrawOp* pDrawOp, RdrBucketType eBucket);
	void AddComputeOp(const RdrComputeOp* pComputeOp, RdrPass ePass);
	void AddLight(const Light* pLight);

	void QueueSkyAndLighting(const AssetLib::SkySettings& rSkySettings, RdrResourceHandle hEnvironmentMapTexArray, float sceneDepthMin, float sceneDepthMax);
	void SetPostProcessingEffects(const AssetLib::PostProcessEffects& postProcFx);

	const RdrDrawOpBucket& GetDrawOpBucket(RdrBucketType eBucketType) const;
	const RdrComputeOpBucket& GetComputeOpBucket(RdrPass ePass) const;

	void SortDrawOps(RdrBucketType eBucketType);

	RdrResourceCommandList& GetResCommandList();

	int GetShadowPassCount() const;
	const Camera& GetShadowCamera(int shadowPassIndex) const;

	void QueueShadowMapPass(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport);
	void QueueShadowCubeMapPass(const PointLight& rLight, RdrDepthStencilViewHandle hDepthView, Rect& viewport);

	const Rect& GetViewport() const;
	const Camera& GetCamera() const;

	const RdrLightResources& GetLightResources() const;
	const RdrGlobalConstants& GetGlobalConstants() const;

private:
	friend Renderer;
	void InitAsPrimary(Camera& rCamera);
	void InitAsOffscreen(const wchar_t* actionName, Camera& rCamera,
		bool enablePostprocessing, const Rect& viewport, const RdrSurface& outputSurface);
	void InitCommon(const wchar_t* actionName, const Rect& viewport, bool enablePostProcessing, const RdrSurface& outputSurface);

	///
	LPCWSTR m_name;

	RdrResourceCommandList m_resourceCommands;

	RdrPassData m_passes[(int)RdrPass::Count];
	RdrDrawOpBucket m_drawOpBuckets[(int)RdrBucketType::Count];
	RdrComputeOpBucket m_computeOpBuckets[(int)RdrPass::Count];

	RdrShadowPass m_shadowPasses[MAX_SHADOW_MAPS_PER_FRAME];
	int m_shadowPassCount;

	Camera m_camera;
	AssetLib::SkySettings m_sky;
	AssetLib::PostProcessEffects m_postProcessEffects;

	Rect m_primaryViewport;

	RdrLightResources m_lightResources;
	RdrLightList m_lights;

	RdrGlobalConstants m_constants;
	RdrGlobalConstants m_uiConstants;

	bool m_bIsCubemapCapture;
	bool m_bEnablePostProcessing;
};

//////////////////////////////////////////////////////////////////////////
// RdrAction inlines

inline void RdrAction::AddDrawOp(const RdrDrawOp* pDrawOp, RdrBucketType eBucket)
{
	RdrDrawBucketEntry entry(pDrawOp);
	m_drawOpBuckets[(int)eBucket].push_back(entry);
}

inline void RdrAction::AddComputeOp(const RdrComputeOp* pComputeOp, RdrPass ePass)
{
	m_computeOpBuckets[(int)ePass].push_back(pComputeOp);
}

inline void RdrAction::AddLight(const Light* pLight)
{
	m_lights.AddLight(pLight);
}

inline const RdrDrawOpBucket& RdrAction::GetDrawOpBucket(RdrBucketType eBucketType) const
{
	return m_drawOpBuckets[(int)eBucketType];
}

inline const RdrComputeOpBucket& RdrAction::GetComputeOpBucket(RdrPass ePass) const
{
	return m_computeOpBuckets[(int)ePass];
}

inline const Rect& RdrAction::GetViewport() const
{
	return m_primaryViewport;
}

inline const Camera& RdrAction::GetCamera() const
{
	return m_camera;
}

inline const RdrLightResources& RdrAction::GetLightResources() const
{
	return m_lightResources;
}

inline const RdrGlobalConstants& RdrAction::GetGlobalConstants() const
{
	return m_constants;
}

inline int RdrAction::GetShadowPassCount() const
{
	return m_shadowPassCount;
}

inline const Camera& RdrAction::GetShadowCamera(int shadowPassIndex) const
{
	return m_shadowPasses[shadowPassIndex].camera;
}
