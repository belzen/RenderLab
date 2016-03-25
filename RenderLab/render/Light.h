#pragma once

#include "Math.h"
#include "RdrContext.h"

struct ID3D11Buffer;
struct ID3D11UnorderedAccessView;
class RdrContext;
class Renderer;
class Camera;
class Scene;

#define MAX_SHADOW_MAPS 10
#define MAX_SHADOW_CUBEMAPS 2
#define USE_SINGLEPASS_SHADOW_CUBEMAP 0

typedef uint RdrResourceHandle;

enum LightType
{
	kLightType_Directional,
	kLightType_Point,
	kLightType_Spot,
	kLightType_Environment,
};

enum CubemapFace
{
	kCubemapFace_Default,
	kCubemapFace_PositiveX = kCubemapFace_Default,
	kCubemapFace_NegativeX,
	kCubemapFace_PositiveY,
	kCubemapFace_NegativeY,
	kCubemapFace_PositiveZ,
	kCubemapFace_NegativeZ
};

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

	///
	Camera MakeCamera(CubemapFace face = kCubemapFace_Default) const;
};

class LightList
{
public:
	LightList();

	void AddLight(Light& light);

	void PrepareDrawForScene(Renderer& rRenderer, const Camera& rCamera, const Scene& scene);

	RdrResourceHandle GetShadowMapDataRes() const { return m_hShadowMapDataRes; }
	RdrResourceHandle GetShadowMapTexArray() const { return m_hShadowMapTexArray; }
	RdrResourceHandle GetShadowCubeMapTexArray() const { return m_hShadowCubeMapTexArray; }
	RdrResourceHandle GetLightListRes() const { return m_hLightListRes; }
	uint GetLightCount() const { return m_lightCount; }
private:
	Light m_lights[1024];
	uint m_lightCount;
	bool m_changed;

	RdrResourceHandle m_hLightListRes;
	RdrResourceHandle m_hShadowMapDataRes;
	RdrResourceHandle m_hShadowMapTexArray;
	RdrResourceHandle m_hShadowCubeMapTexArray;
	RdrDepthStencilView m_shadowMapDepthViews[MAX_SHADOW_MAPS];
#if USE_SINGLEPASS_SHADOW_CUBEMAP
	RdrRenderTargetView m_shadowCubeMapViews[MAX_SHADOW_CUBEMAPS * 6];
#else
	RdrDepthStencilView m_shadowCubeMapDepthViews[MAX_SHADOW_CUBEMAPS * 6];
#endif
};
