#pragma once

#include "Math.h"
#include "RdrContext.h"

struct ID3D11Buffer;
struct ID3D11UnorderedAccessView;
class RdrContext;
class Renderer;
class Camera;
class Scene;

#define MAX_SHADOWMAPS 10

typedef uint RdrResourceHandle;

enum LightType
{
	kLightType_Directional,
	kLightType_Point,
	kLightType_Spot,
	kLightType_Environment,
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
	Camera MakeCamera(uint face) const;
};

class LightList
{
public:
	LightList();

	void AddLight(Light& light);

	void PrepareDrawForScene(Renderer& rRenderer, const Camera& rCamera, const Scene& scene);

	RdrResourceHandle GetShadowMapDataRes() const { return m_hShadowMapDataRes; }
	RdrResourceHandle GetShadowMapTexArray() const { return m_hShadowMapTexArray; }
	RdrResourceHandle GetLightListRes() const { return m_hLightListRes; }
	uint GetLightCount() const { return m_lightCount; }
private:
	Light m_lights[1024];
	uint m_lightCount;
	bool m_changed;

	RdrResourceHandle m_hLightListRes;
	RdrResourceHandle m_hShadowMapDataRes;
	RdrResourceHandle m_hShadowMapTexArray;
	RdrDepthStencilView m_shadowMapDepthViews[MAX_SHADOWMAPS];
};
