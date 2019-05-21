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
class RdrContext;
class RdrProfiler;
class RdrDrawState;
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
		if (rLeft.sortKey.compare.val != rRight.sortKey.compare.val)
			return rLeft.sortKey.compare.val < rRight.sortKey.compare.val;
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
};

struct RdrPassData
{
	RdrRenderTargetViewHandle ahRenderTargets[MAX_RENDER_TARGETS];
	RdrDepthStencilViewHandle hDepthTarget;

	Rect viewport;

	RdrShaderMode shaderMode;

	bool bEnabled;
	bool bClearRenderTargets;
	bool bClearDepthTarget;
};

struct RdrShadowPass
{
	RdrPassData               passData;
	RdrGlobalConstants        constants;
	Camera                    camera;
	RdrDepthStencilViewHandle hDepthView;
};

struct RdrActionSurfaces
{
	Vec2 viewportSize;
	RdrRenderTarget colorBuffer;
	RdrRenderTarget albedoBuffer;
	RdrRenderTarget normalBuffer;
	RdrResourceHandle hDepthBuffer;
	RdrDepthStencilViewHandle hDepthStencilView;
};

class RdrAction
{
public:
	static void InitSharedData(RdrContext* pContext, const InputManager* pInputManager);
	static RdrAction* CreatePrimary(Camera& rCamera);
	static RdrAction* CreateOffscreen(const wchar_t* actionName, Camera& rCamera,
		bool enablePostprocessing, const Rect& viewport, RdrRenderTargetViewHandle hOutputTarget);

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

	int GetShadowPassCount() const;
	const Camera& GetShadowCamera(int shadowPassIndex) const;

	void QueueShadowMapPass(const Camera& rCamera, RdrDepthStencilViewHandle hDepthView, Rect& viewport);

	const Rect& GetViewport() const;
	const Camera& GetCamera() const;

	const RdrActionLightResources& GetLightResources() const;
	const RdrGlobalConstants& GetGlobalConstants() const;

	RdrResourceHandle GetPrimaryDepthBuffer() const;

	// Issue action's draw commands to the render device.
	void Draw(RdrContext* pContext, RdrDrawState* pDrawState, RdrProfiler* pProfiler);

	// Idle draw.  The window isn't visible and there's no point issuing draw calls.
	// Only issues non-transient commands which would cause breakages if they were skipped (like resource updates).
	void DrawIdle(RdrContext* pContext);

private:
	void InitAsPrimary(Camera& rCamera);
	void InitAsOffscreen(const wchar_t* actionName, Camera& rCamera,
		bool enablePostprocessing, const Rect& viewport, RdrRenderTargetViewHandle hOutputTarget);
	void InitCommon(const wchar_t* actionName, bool isPrimaryAction, const Rect& viewport, bool enablePostProcessing, RdrRenderTargetViewHandle hOutputTarget);

	//
	void DrawPass(RdrPass ePass);
	void DrawShadowPass(int shadowPassIndex);
	void DrawBucket(const RdrPassData& rPass, const RdrDrawOpBucket& rBucket, const RdrGlobalConstants& rGlobalConstants);
	void DrawGeo(const RdrPassData& rPass, const RdrGlobalConstants& rGlobalConstants, const RdrDrawOp* pDrawOp, uint instanceCount);
	void DispatchCompute(const RdrComputeOp* pComputeOp);

	///
	LPCWSTR m_name;

	RdrRenderTargetViewHandle m_hOutputTarget;
	RdrActionSurfaces m_surfaces;

	RdrPassData m_passes[(int)RdrPass::Count];
	RdrDrawOpBucket m_drawOpBuckets[(int)RdrBucketType::Count];
	RdrComputeOpBucket m_computeOpBuckets[(int)RdrPass::Count];

	RdrShadowPass m_shadowPasses[MAX_SHADOW_MAPS_PER_FRAME];
	int m_shadowPassCount;

	Camera m_camera;
	AssetLib::SkySettings m_sky;
	AssetLib::PostProcessEffects m_postProcessEffects;

	Rect m_primaryViewport;

	// Debug texture to copy to the output surface.
	// This will typically be a texture that is written to as part of the action.
	RdrResourceHandle m_hDebugCopyTexture;

	RdrActionLightResources m_lightResources;
	RdrLightList m_lights;

	RdrGlobalConstants m_constants;
	RdrGlobalConstants m_uiConstants;

	bool m_bEnablePostProcessing;

	// Active render state data. Filled in during Draw() and cleared before returning.  
	// Stored here just to simplify argument passing to child draw functions.
	RdrContext* m_pContext;
	RdrDrawState* m_pDrawState;
	RdrProfiler* m_pProfiler;
};

//////////////////////////////////////////////////////////////////////////
// RdrAction inlines

inline void RdrAction::AddDrawOp(const RdrDrawOp* pDrawOp, RdrBucketType eBucket)
{
	assert(eBucket < RdrBucketType::Count);

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

inline const RdrActionLightResources& RdrAction::GetLightResources() const
{
	return m_lightResources;
}

inline const RdrGlobalConstants& RdrAction::GetGlobalConstants() const
{
	return m_constants;
}

inline RdrResourceHandle RdrAction::GetPrimaryDepthBuffer() const
{
	return m_surfaces.hDepthBuffer;
}

inline int RdrAction::GetShadowPassCount() const
{
	return m_shadowPassCount;
}

inline const Camera& RdrAction::GetShadowCamera(int shadowPassIndex) const
{
	return m_shadowPasses[shadowPassIndex].camera;
}
