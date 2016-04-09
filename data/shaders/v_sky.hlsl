#include "vs_constants.h"
#include "vs_output.hlsli"

cbuffer PerAction : register(b0)
{
	VsPerAction cbPerAction;
};

cbuffer PerObject : register(b1)
{
	float4x4 mtxWorld;
};

struct VertexInput
{
	float3 position : POSITION;
};

VsOutputSky main(VertexInput input)
{
	VsOutputSky output = (VsOutputSky)0;

	output.direction = input.position;

	output.position = mul(float4(input.position, 1.f), mtxWorld);
	output.position = mul(output.position, cbPerAction.mtxViewProj).xyww;

	return output;
}