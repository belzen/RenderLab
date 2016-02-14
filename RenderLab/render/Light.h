#pragma once

#include "Math.h"

struct ID3D11Buffer;
struct ID3D11UnorderedAccessView;
struct RdrContext;

typedef uint RdrTextureHandle;

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
	float innerConeAngle; // Angle where light begins to fall off
	float outerConeAngle; // No more light

	uint castsShadows;
};

struct LightList
{
	RdrTextureHandle hResource;
	uint lightCount;

	static LightList Create(RdrContext* pContext, const Light* aLights, int numLights);
};
