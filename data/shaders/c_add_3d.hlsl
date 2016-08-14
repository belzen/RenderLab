#include "c_constants.h"

Texture3D<float4> texInput1 : register(t0);
Texture3D<float4> texInput2 : register(t1);
RWTexture3D<float4> texOutput : register(u0);

[numthreads(ADD_THREADS_X, ADD_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	texOutput[globalId.xyz] = texInput1[globalId.xyz] + texInput2[globalId.xyz];
}
