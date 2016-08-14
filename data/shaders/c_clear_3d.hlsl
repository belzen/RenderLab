#include "c_constants.h"

RWTexture3D<float4> g_texture : register(u0);

[numthreads(CLEAR_THREADS_X, CLEAR_THREADS_Y, 1)]
void main( uint3 globalId : SV_DispatchThreadID )
{
	g_texture[globalId.xyz] = 0.f;
}