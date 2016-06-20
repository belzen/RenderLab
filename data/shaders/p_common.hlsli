#include "p_constants.h"
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