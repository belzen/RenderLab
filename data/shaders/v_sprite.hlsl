#include "vs_common.hlsli"

cbuffer PerObject : register(b1)
{
	float3 screen_pos;
	float2 scale;
	float alpha;
};

struct VertexInput
{
	float2 position : POSITION;
	float2 texcoords : TEXCOORD0;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float2 texcoords : TEXCOORD0;
	float alpha : TEXCOORD1;
};

VSOutput main(VertexInput input)
{
	VSOutput output;

	float4 pos = float4(input.position * scale.xy + screen_pos.xy, screen_pos.z, 1.f);
	output.position = mul(pos, cbPerFrame.mtxViewProj);

	output.texcoords = input.texcoords;
	output.alpha = alpha;
	return output;
}