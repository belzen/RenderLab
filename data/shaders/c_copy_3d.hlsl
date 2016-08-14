#include "c_constants.h"

Texture3D<float4> g_texInput : register(t0);
RWTexture3D<float4> g_texOutput : register(u0);

[numthreads(COPY_THREADS_X, COPY_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	g_texOutput[globalId.xyz] = g_texInput[globalId.xyz];
}