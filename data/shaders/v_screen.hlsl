#include "v_output.hlsli"
#include "v_common.hlsli"

struct VertexInput
{
	float2 position : POSITION;
	float2 texcoords : TEXCOORD0;
};

VsOutputSprite main(VertexInput input)
{
	VsOutputSprite output = (VsOutputSprite)0;

	// Geo is already in screenspace, just pass through.
	output.position = float4(input.position.xy, 0.f, 1.f);
	output.texcoords = input.texcoords;
	
	return output;
}