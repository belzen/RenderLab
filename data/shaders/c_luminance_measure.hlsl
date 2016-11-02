#include "p_util.hlsli"
#include "p_constants.h"
#include "c_constants.h"

// 16x16 tiles, 4 samples each
#define GroupSizeX 8
#define GroupSizeY 8
#define ThreadCount GroupSizeX * GroupSizeY

cbuffer ToneMapInputBuffer : register(b0)
{
	ToneMapInputParams cbTonemapInput;
}

Texture2D<float4> texInput : register(t0);

#if STEP_FINAL
RWStructuredBuffer<ToneMapOutputParams> bufToneMapOutput : register(u0);
#else
RWTexture2D<float4> texLumOutput : register(u0);
#endif

groupshared float4 grp_logLum[GroupSizeX * GroupSizeY];

[numthreads(GroupSizeX, GroupSizeY, 1)]
void main( uint3 globalId : SV_DispatchThreadID, uint3 groupId : SV_GroupId, uint localIdx : SV_GroupIndex )
{
	uint2 samplePos = globalId.xy * 2;

	// TODO: Remove need for multiple shrink steps.  One step can shrink from full-res to small tex (exactly like bloom, need to merge shaders).
	//			Then a separate shader can do a single pass to find the final average and store it in tonemap output
#if STEP_ONE
	float4 c00 = texInput[samplePos + uint2(0, 0)];
	float4 c10 = texInput[samplePos + uint2(1, 0)];
	float4 c01 = texInput[samplePos + uint2(0, 1)];
	float4 c11 = texInput[samplePos + uint2(1, 1)];

	float4 lum;
	lum.x = getLuminance(c00.rgb);
	lum.y = getLuminance(c01.rgb);
	lum.z = getLuminance(c11.rgb);
	lum.w = getLuminance(c10.rgb);
	
	grp_logLum[localIdx] = (lum <= 0.f) ? 0 : log2(lum);

#else
	float4 lum = texInput[samplePos + uint2(0, 0)];
	lum += texInput[samplePos + uint2(1, 1)];
	lum += texInput[samplePos + uint2(0, 1)];
	lum += texInput[samplePos + uint2(1, 0)];

	grp_logLum[localIdx] = lum / 4;
#endif

	GroupMemoryBarrierWithGroupSync();

	for (uint i = ThreadCount / 2; i > 0; i >>= 1)
	{
		if (localIdx < i)
		{
			grp_logLum[localIdx] += grp_logLum[localIdx + i];
		}

		GroupMemoryBarrierWithGroupSync();
	}

	if (localIdx == 0)
	{
		uint2 inputSize;
		texInput.GetDimensions(inputSize.x, inputSize.y);

		uint2 sampleSize = min( (inputSize - samplePos), 2 * uint2(GroupSizeX, GroupSizeY));
		sampleSize = max(sampleSize, uint2(1, 1));
		uint numSamples = sampleSize.x * sampleSize.y;

#if STEP_FINAL
		// Calculate final average luminance and setup tonemapping params.
		float avgLum = (grp_logLum[0].x + grp_logLum[0].y + grp_logLum[0].z + grp_logLum[0].w) / numSamples;
		avgLum = exp2(avgLum);

		// Decay luminance
		// todo: exp decay should be based on time, right now it's fixed per frame
		float adaptedLum = bufToneMapOutput[0].adaptedLum + (avgLum - bufToneMapOutput[0].adaptedLum) * (1.f - exp(0.01f * -0.2f));

		bufToneMapOutput[0].linearExposure = clamp(cbTonemapInput.middleGrey / adaptedLum, cbTonemapInput.minExposure, cbTonemapInput.maxExposure);
		bufToneMapOutput[0].white = cbTonemapInput.white;
		bufToneMapOutput[0].adaptedLum = adaptedLum;
#else
		texLumOutput[groupId.xy] = grp_logLum[0] / (numSamples / 4);
#endif
	}
}
