#include "v_constants.h"
#include "v_output.hlsli"

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

	output.position = float4(input.position + cbPerAction.cameraPosition, 1);
	output.position = mul(output.position, cbPerAction.mtxViewProj).xyww;

	output.position_ws = float4(input.position + cbPerAction.cameraPosition, 1);

	return output;
}