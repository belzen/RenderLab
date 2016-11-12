#include "p_constants.h"
#include "c_constants.h"
#include "p_util.hlsli"
#include "light_types.h"

cbuffer PerAction : register(b0)
{
	PsPerAction cbPerAction;
};

cbuffer GlobalLightsBuffer : register(b1)
{
	GlobalLightData cbGlobalLights;
}

cbuffer AtmosphereParamsBuffer : register(b2)
{
	AtmosphereParams cbAtmosphere;
}

cbuffer MaterialParamsBuffer : register(b3)
{
	MaterialParams cbMaterial;
}


#if DEPTH_ONLY
#define PsOutput void
#else
struct PsOutput
{
	float4 color : SV_TARGET;
};
#endif