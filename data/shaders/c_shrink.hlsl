#include "p_util.hlsli"
#include "p_constants.h"
#include "c_constants.h"

// Input resources
Texture2D<float4> texInput: register(t0);

#if BLOOM_HIGHPASS
cbuffer ToneMapInputBuffer : register(b0)
{
	ToneMapInputParams cbTonemapInput;
}
StructuredBuffer<ToneMapOutputParams> bufToneMapParams : register(t1);
static const float g_bloomSmoothRange = 1.0f;
#endif

// Output resources
RWTexture2D<float4> texOutput : register(u0);

///
[numthreads(SHRINK_THREADS_X, SHRINK_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID, uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex)
{
	uint2 samplePos = globalId.xy * 2;

	float4 c00 = texInput[samplePos + uint2(0, 0)];
	float4 c10 = texInput[samplePos + uint2(1, 0)];
	float4 c01 = texInput[samplePos + uint2(0, 1)];
	float4 c11 = texInput[samplePos + uint2(1, 1)];

#if BLOOM_HIGHPASS
	float linearExposure = bufToneMapParams[0].linearExposure;
	c00 *= saturate((getLuminance(c00.rgb) * linearExposure - cbTonemapInput.bloomThreshold + g_bloomSmoothRange) / g_bloomSmoothRange);
	c10 *= saturate((getLuminance(c10.rgb) * linearExposure - cbTonemapInput.bloomThreshold + g_bloomSmoothRange) / g_bloomSmoothRange);
	c01 *= saturate((getLuminance(c01.rgb) * linearExposure - cbTonemapInput.bloomThreshold + g_bloomSmoothRange) / g_bloomSmoothRange);
	c11 *= saturate((getLuminance(c11.rgb) * linearExposure - cbTonemapInput.bloomThreshold + g_bloomSmoothRange) / g_bloomSmoothRange);
#endif

	texOutput[globalId.xy] = (c00 + c10 + c01 + c11) * 0.25f;
}