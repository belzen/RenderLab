#include "c_constants.h"

cbuffer AddParamsBuffer : register(c0)
{
	AddParams cbAdd;
}

Texture2D texInput1 : register(t0);
Texture2D texInput2 : register(t1);
SamplerState samInput : register(s0);

RWTexture2D<float4> texOutput : register(u0);

[numthreads(ADD_THREADS_X, ADD_THREADS_Y, 1)]
void main( uint3 globalId : SV_DispatchThreadID )
{
	texOutput[globalId.xy] = 0.5f * (texInput1[globalId.xy] + texInput2.SampleLevel(samInput, (globalId.xy + 0.5f) / float2(cbAdd.width, cbAdd.height), 0));
}