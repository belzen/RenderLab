#include "p_util.hlsli"
#include "p_constants.h"
#include "c_constants.h"

#define GroupSizeX 8
#define GroupSizeY 8

cbuffer ToneMapInputBuffer : register(b0)
{
	ToneMapInputParams cbTonemapInput;
}

Texture2D<float4> texColor : register(t0);
StructuredBuffer<ToneMapOutputParams> bufToneMapParams : register(t1);

RWTexture2D<float4> texBloom2 : register(u0);
RWTexture2D<float4> texBloom4 : register(u1);
RWTexture2D<float4> texBloom8 : register(u2);
RWTexture2D<float4> texBloom16 : register(u3);

groupshared float4 grp_colors[GroupSizeX * GroupSizeY];

static const float g_bloomSmoothRange = 0.1f;

[numthreads(GroupSizeX, GroupSizeY, 1)]
void main( uint3 globalId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex )
{
	uint2 samplePos = globalId.xy * 2;

	// TODO: Merge this in with the luminance measure step.  This is doing the exact same thing as the first lum measure pass.
	// todo: use sample instead of load, gives auto-box filter?
	float4 c00 = texColor[samplePos + uint2(0, 0)];
	float4 c10 = texColor[samplePos + uint2(1, 0)];
	float4 c01 = texColor[samplePos + uint2(0, 1)];
	float4 c11 = texColor[samplePos + uint2(1, 1)];

	float linearExposure = bufToneMapParams[0].linearExposure;
	c00 *= saturate((getLuminance(c00.rgb) * linearExposure - cbTonemapInput.bloomThreshold + g_bloomSmoothRange) / g_bloomSmoothRange);
	c10 *= saturate((getLuminance(c10.rgb) * linearExposure - cbTonemapInput.bloomThreshold + g_bloomSmoothRange) / g_bloomSmoothRange);
	c01 *= saturate((getLuminance(c01.rgb) * linearExposure - cbTonemapInput.bloomThreshold + g_bloomSmoothRange) / g_bloomSmoothRange);
	c11 *= saturate((getLuminance(c11.rgb) * linearExposure - cbTonemapInput.bloomThreshold + g_bloomSmoothRange) / g_bloomSmoothRange);

	float4 color = (c00 + c10 + c01 + c11) / 4;

	texBloom2[globalId.xy] = color;
	grp_colors[localIdx] = color;

	GroupMemoryBarrierWithGroupSync();

	if (localIdx < 16)
	{
		uint y = localIdx / 4;
		uint x = localIdx % 4;

		uint idx = y * 2 * 8 + x * 2;
		texBloom4[groupId.xy * 4 + uint2(x, y)] = (grp_colors[idx] + grp_colors[idx + 1] + grp_colors[idx + 8] + grp_colors[idx + 9]) / 4;
	}

	if (localIdx < 4)
	{
		uint y = localIdx / 2;
		uint x = localIdx % 2;

		uint idx = y * 2 * 4 + x * 2;
		texBloom8[groupId.xy * 2 + uint2(x, y)] = (grp_colors[idx] + grp_colors[idx + 1] + grp_colors[idx + 4] + grp_colors[idx + 5]) / 4;
	}

	if (localIdx == 0)
	{
		texBloom16[groupId.xy] = (grp_colors[0] + grp_colors[1] + grp_colors[2] + grp_colors[3]) / 4;
	}
}