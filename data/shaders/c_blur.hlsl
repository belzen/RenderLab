#include "c_constants.h"

Texture2D texInput : register(t0);
RWTexture2D<float4> texOutput : register(u0);

static const float g_blurWeights[9] = {
	0.01611328125f,
	0.0537109375f,
	0.120849609375f,
	0.193359375f,
	0.2255859375f,
	0.193359375f,
	0.120849609375f,
	0.0537109375f,
	0.01611328125f
};

#if BLUR_HORIZONTAL

[numthreads(1, BLUR_THREAD_COUNT, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	float4 inputs[BLUR_SIZE + BLUR_TEXELS_PER_THREAD];

	for (int i = 0; i < BLUR_SIZE + BLUR_TEXELS_PER_THREAD; ++i)
	{
		inputs[i] = texInput[uint2(globalId.x * BLUR_TEXELS_PER_THREAD + i - BLUR_HALFSIZE, globalId.y)];
	}

	for (int x = 0; x < BLUR_TEXELS_PER_THREAD; ++x)
	{
		float4 blurred = 0;
		for (int i = 0; i < BLUR_SIZE; ++i)
		{
			blurred += inputs[(x+i)] * g_blurWeights[i];
		}
		texOutput[uint2(globalId.x * BLUR_TEXELS_PER_THREAD + x, globalId.y)] = blurred;
	}
}

#else //BLUR_VERTICAL

[numthreads(BLUR_THREAD_COUNT, 1, 1)]
void main(uint3 globalId : SV_DispatchThreadID)
{
	float4 inputs[BLUR_SIZE + BLUR_TEXELS_PER_THREAD];

	for (int i = 0; i < BLUR_SIZE + BLUR_TEXELS_PER_THREAD; ++i)
	{
		inputs[i] = texInput[uint2(globalId.x, globalId.y * BLUR_TEXELS_PER_THREAD + i - BLUR_HALFSIZE)];
	}

	for (int y = 0; y < BLUR_TEXELS_PER_THREAD; ++y)
	{
		float4 blurred = 0;
		for (int i = 0; i < BLUR_SIZE; ++i)
		{
			blurred += inputs[(y+i)] * g_blurWeights[i];
		}
		texOutput[uint2(globalId.x, globalId.y * BLUR_TEXELS_PER_THREAD + y)] = blurred;
	}
}

#endif