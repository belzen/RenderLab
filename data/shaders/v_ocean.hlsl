#include "v_constants.h"
#include "v_output.hlsli"

cbuffer PerAction : register(b0)
{
	VsPerAction cbPerAction;
};

cbuffer PerObject : register(b1)
{
	VsPerObject cbPerObject;
};

struct VertexInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float jacobian : TEXCOORD0;
};

VsOutputOcean main(VertexInput input)
{
	VsOutputOcean output = (VsOutputOcean)0;

	float4x4 mtxWorld = cbPerObject.mtxWorld;
	output.position_ws = mul(float4(input.position, 1), mtxWorld);

	output.position = mul(output.position_ws, cbPerAction.mtxViewProj);

	output.normal = input.normal;

	float2 noise_gradient = 0.3f * input.normal.xz;
	float turbulence = max(2.f - input.jacobian + dot(abs(noise_gradient), float2(1.2f, 1.2f)), 0.0);
	output.color_mod = 1.f + 3.f * smoothstep(1.2f, 1.8f, turbulence);
	
	return output;
}