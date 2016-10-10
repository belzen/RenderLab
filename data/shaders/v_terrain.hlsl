#include "v_constants.h"
#include "v_output.hlsli"

cbuffer PerAction : register(b0)
{
	VsPerAction cbPerAction;
};

struct VertexInput
{
	float2 position : POSITION;
	// Instance params
	float4 offset_scale_lod : TEXCOORD0;
};

VsOutputTerrain main(VertexInput input)
{
	VsOutputTerrain output = (VsOutputTerrain)0;

	float2 pos = input.position * input.offset_scale_lod.z + input.offset_scale_lod.xy;
	output.position_ws = float4(pos.x, 0.f, pos.y, 1.f);
	output.lod = input.offset_scale_lod.w;

	return output;
}