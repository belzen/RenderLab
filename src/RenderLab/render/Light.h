#pragma once

#include "Math.h"
#include "RdrContext.h"
#include "AssetLib/SceneAsset.h"
#include "RdrShaderTypes.h"
#include "../../data/shaders/light_types.h"

struct ID3D11Buffer;
struct ID3D11UnorderedAccessView;
class RdrContext;
class Renderer;
class Camera;
class Scene;

#define MAX_SHADOW_MAPS_PER_FRAME 10
#define USE_SINGLEPASS_SHADOW_CUBEMAP 1

struct Light
{
	LightType type;
	Vec3 position;
	Vec3 direction;
	Vec3 color;
	float radius;
	float innerConeAngle; // Angle where light begins to fall off
	float outerConeAngle; // No more light
	uint shadowMapIndex;
	uint castsShadows;
};

class LightList
{
public:
	LightList();

	void Cleanup();

	void AddLight(const Light& light);

	void UpdateSunLight(const Light& rSunLight);

	void PrepareDraw(Renderer& rRenderer, const Camera& rCamera, const float sceneDepthMin, const float sceneDepthMax);

	RdrResourceHandle GetShadowMapDataRes() const;
	RdrResourceHandle GetShadowMapTexArray() const;
	RdrResourceHandle GetShadowCubeMapTexArray() const;

	RdrResourceHandle GetSpotLightListRes() const;
	RdrResourceHandle GetPointLightListRes() const;
	RdrConstantBufferHandle GetDirectionalLightListCb() const;

	uint GetLightCount() const;
	uint GetSpotLightCount() const;
	uint GetPointLightCount() const;

private:
	Light m_lights[2048];
	uint m_lightCount;
	uint m_prevLightCount;

	RdrConstantBufferHandle m_hDirectionalLightListCb;
	RdrResourceHandle m_hSpotLightListRes;
	RdrResourceHandle m_hPointLightListRes;

	uint m_numSpotLights;
	uint m_numPointLights;

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

inline RdrResourceHandle LightList::GetSpotLightListRes() const
{ 
	return m_hSpotLightListRes; 
}

inline RdrResourceHandle LightList::GetPointLightListRes() const
{
	return m_hPointLightListRes;
}

inline RdrConstantBufferHandle LightList::GetDirectionalLightListCb() const
{
	return m_hDirectionalLightListCb;
}

inline uint LightList::GetSpotLightCount() const
{
	return m_numSpotLights;
}

inline uint LightList::GetPointLightCount() const
{
	return m_numPointLights;
}

inline uint LightList::GetLightCount() const
{ 
	return m_lightCount; 
}
