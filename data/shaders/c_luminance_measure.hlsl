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

Texture2D texInput : register(t0);

#if STEP_FINAL
RWStructuredBuffer<ToneMapOutputParams> bufToneMapOutput : register(u0);
#else
RWTexture2D<float4> texOutput : register(u0);
#endif

groupshared float4 grp_logLum[GroupSizeX * GroupSizeY];

[numthreads(GroupSizeX, GroupSizeY, 1)]
void main( uint3 globalId : SV_DispatchThreadID, uint3 groupId : SV_GroupId, uint localIdx : SV_GroupIndex )
{
	uint2 samplePos = globalId.xy * 2;

#if STEP_ONE
	float4 lum;
	lum.x = getLuminance( texInput[samplePos + uint2(0, 0)].rgb );
	lum.w = getLuminance( texInput[samplePos + uint2(1, 1)].rgb );
	lum.y = getLuminance( texInput[samplePos + uint2(0, 1)].rgb );
	lum.z = getLuminance( texInput[samplePos + uint2(0, 1)].rgb );

	grp_logLum[localIdx] = log2(lum);
#else
	float4 lum = texInput[samplePos + uint2(0, 0)];
	lum += texInput[samplePos + uint2(1, 1)];
	lum += texInput[samplePos + uint2(0, 1)];
	lum += texInput[samplePos + uint2(0, 1)];

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

		uint2 sampleSize = min( (inputSize - samplePos) / 2, uint2(GroupSizeX, GroupSizeY));
		sampleSize = max(sampleSize, uint2(1, 1));
		uint numSamples = sampleSize.x * sampleSize.y;

#if STEP_FINAL
		// Calculate final average luminance and setup tonemapping params.
		float avgLum = (grp_logLum[0].x + grp_logLum[0].y + grp_logLum[0].z + grp_logLum[0].w) / (4 * numSamples);
		avgLum = exp2(avgLum);

		float avgLumScaled = cbTonemapInput.middleGrey / avgLum;

		bufToneMapOutput[0].linearExposure = avgLumScaled;
		bufToneMapOutput[0].white = cbTonemapInput.white;
		bufToneMapOutput[0].lumAvg = avgLum;
#else
		texOutput[groupId.xy] = grp_logLum[0] / numSamples;
#endif
	}
}
