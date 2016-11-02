#include "p_constants.h"
#include "c_constants.h"
#include "p_util.hlsli"
#include "light_types.h"

cbuffer PerAction : register(b0)
{
	PsPerAction cbPerAction;
};

cbuffer DirectionalLightsBuffer : register(b1)
{
	DirectionalLightList cbDirectionalLights;
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