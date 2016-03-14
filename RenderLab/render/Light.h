#pragma once

#include "Math.h"

struct ID3D11Buffer;
struct ID3D11UnorderedAccessView;
class RdrContext;
class Renderer;

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
	Matrix44 GetViewMatrix(int face) const;
	Matrix44 GetProjMatrix() const;
};

class LightList
{
public:
	LightList();

	void AddLight(Light& light);

	void PrepareDraw(Renderer& rRenderer);

	RdrResourceHandle GetShadowMapDataRes() const { return m_hShadowMapDataRes; }
	RdrResourceHandle GetShadowMapTexArray() const { return m_hShadowMapTexArray; }
	RdrResourceHandle GetLightListRes() const { return m_hLightListRes; }
	uint GetLightCount() const { return m_lightCount; }
private:
	Light m_lights[1024];
	uint m_lightCount;
	bool m_changed;
	bool m_needsRecreate;

	RdrResourceHandle m_hLightListRes;
	RdrResourceHandle m_hShadowMapDataRes;
	RdrResourceHandle m_hShadowMapTexArray;
};
