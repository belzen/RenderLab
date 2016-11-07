#include "c_constants.h"

cbuffer Blend2dParamsBuffer : register(b0)
{
	Blend2dParams cbBlend;
}

Texture2D<float4> texInput1 : register(t0);
Texture2D<float4> texInput2 : register(t1);
SamplerState samInput : register(s0);

RWTexture2D<float4> texOutput : register(u0);

[numthreads(BLEND_THREADS_X, BLEND_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	float4 c1 = texInput1[globalId.xy];
	float4 c2 = texInput2.SampleLevel(samInput, (globalId.xy + 0.5f) / cbBlend.size1, 0);
	texOutput[globalId.xy] = c1 * cbBlend.weight1 + c2 * cbBlend.weight2;
}