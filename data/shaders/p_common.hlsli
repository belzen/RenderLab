#include "p_constants.h"
#include "c_constants.h"
#include "p_util.hlsli"
#include "light_types.h"

cbuffer PerAction : register(b0)
{
	PsPerAction cbPerAction;
};

cbuffer MaterialParamsBuffer : register(b1)
{
	MaterialParams cbMaterial;
}

cbuffer GlobalLightsBuffer : register(b2)
{
	GlobalLightData cbGlobalLights;
}

cbuffer AtmosphereParamsBuffer : register(b3)
{
	AtmosphereParams cbAtmosphere;
}


#if DEPTH_ONLY
#define PsOutput void
#else
struct PsOutput
{
	float4 color : SV_TARGET0;
	float3 albedo : SV_TARGET1;
	float3 normal : SV_TARGET2;
};
#endif