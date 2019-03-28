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
	float2 position : POSITION;
	float2 texcoord : TEXCOORD0;
};

VsOutputWater main(VertexInput input)
{
	VsOutputWater output = (VsOutputWater)0;

	float4 localPosition = float4(input.position.x, 0.f, input.position.y, 1.f);
	float4x4 mtxWorld = cbPerObject.mtxWorld;
	output.position_ws = mul(localPosition, mtxWorld);

	output.position = mul(output.position_ws, cbPerAction.mtxViewProj);

	output.texcoord = input.texcoord;

	return output;
}