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
};

VsOutputDecal main(VertexInput input)
{
	VsOutputDecal output = (VsOutputDecal)0;

	float4 positionWorld = mul(float4(input.position, 1), cbPerObject.mtxWorld);
	output.position = mul(positionWorld, cbPerAction.mtxViewProj);
	output.position_vs = mul(positionWorld, cbPerAction.mtxView);
	output.position_cs = output.position;

	return output;
}