#pragma once

#include "Math.h"
#include "RdrContext.h"
#include "AssetLib/SceneAsset.h"

struct ID3D11Buffer;
struct ID3D11UnorderedAccessView;
class RdrContext;
class Renderer;
class Camera;
class Scene;

#define MAX_SHADOW_MAPS 10
#define MAX_SHADOW_CUBEMAPS 2
#define USE_SINGLEPASS_SHADOW_CUBEMAP 1

// WARNING - Must match struct in light_inc.hlsli
struct Light
{
	LightType type;
	Vec3 position;
	Vec3 direction;
	float radius;
	Vec3 color;

	// Spot light only
	float innerConeAngleCos; // Cosine of angle where light begins to fall off
	float outerConeAngleCos; // No more light

	uint castsShadows;
	uint shadowMapIndex;
};

class LightList
{
public:
	LightList();

	void Cleanup();

	void AddLight(Light& light);

	void PrepareDrawForScene(Renderer& rRenderer, const Scene& scene);

	RdrResourceHandle GetShadowMapDataRes() const;
	RdrResourceHandle GetShadowMapTexArray() const;
	RdrResourceHandle GetShadowCubeMapTexArray() const;
	RdrResourceHandle GetLightListRes() const;
	uint GetLightCount() const;

private:
	Light m_lights[1024];
	uint m_lightCount;
	uint m_prevLightCount;

	RdrResourceHandle m_hLightListRes;
	RdrResourceHandle m_hShadowMapDataRes;
	RdrResourceHandle m_hShadowMapTexArray;
	RdrResourceHandle m_hShadowCubeMapTexArray;
	RdrDepthStencilViewHandle m_shadowMapDepthViews[MAX_SHADOW_MAPS];
#if USE_SINGLEPASS_SHADOW_CUBEMAP
	RdrDepthStencilViewHandle m_shadowCubeMapDepthViews[MAX_SHADOW_CUBEMAPS];
#else
	RdrDepthStencilViewHandle m_shadowCubeMapDepthViews[MAX_SHADOW_CUBEMAPS * CubemapFace::Count];
#endif
};

inline RdrResourceHandle LightList::GetShadowMapDataRes() const
{ 
	return m_hShadowMapDataRes; 
}

inline RdrResourceHandle LightList::GetShadowMapTexArray() const
{ 
	return m_hShadowMapTexArray; 
}

inline RdrResourceHandle LightList::GetShadowCubeMapTexArray() const
{ 
	return m_hShadowCubeMapTexArray; 
}

inline RdrResourceHandle LightList::GetLightListRes() const
{ 
	return m_hLightListRes; 
}

inline uint LightList::GetLightCount() const
{ 
	return m_lightCount; 
}
