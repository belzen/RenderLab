#include "c_constants.h"

Texture2D<float4> texInput1 : register(t0);
Texture2D<float4> texInput2 : register(t1);
RWTexture2D<float4> texOutput : register(u0);

[numthreads(ADD_THREADS_X, ADD_THREADS_Y, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	texOutput[globalId.xy] = texInput1[globalId.xy] + texInput2[globalId.xy];
}
