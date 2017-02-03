#include "p_constants.h"
#include "v_output.hlsli"
#include "light_types.h"

cbuffer SsaoParamsBuffer : register(c0)
{
	SsaoParams cbSsaoParams;
};

Texture2D<float> g_texOcclusion : register(t0);
SamplerState g_samClamp : register(s0);

// Special blur specific to SSAO
// This blends the pixels of the noise SSAO buffer based on the dimensions of the noise texture.
float main(VsOutputSprite input) : SV_TARGET
{
	float occlusion = 0.f;
	float2 offset = (cbSsaoParams.blurSize * -0.5f + 0.5f) * cbSsaoParams.texelSize;
	for (float x = 0; x < cbSsaoParams.blurSize; ++x)
	{
		for (int y = 0; y < cbSsaoParams.blurSize; ++y)
		{
			occlusion += g_texOcclusion.Sample(g_samClamp, input.texcoords + offset + cbSsaoParams.texelSize * float2(x, y));
		}
	}

	occlusion /= (cbSsaoParams.blurSize * cbSsaoParams.blurSize);
	return occlusion;
}