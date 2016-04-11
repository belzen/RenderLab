#include "v_output.hlsli"
#include "p_common.hlsli"

Texture2D texColor : register(t0);
SamplerState sampColor : register(s0);

StructuredBuffer<ToneMapParams> bufToneMapParams : register(t1);

float4 main(VsOutputSprite input) : SV_TARGET
{
	float4 color = texColor.Sample(sampColor, input.texcoords);

	ToneMapParams tonemap = bufToneMapParams[0];
	// todo
	//color *= tonemap.linearExposure;
	//color = (color * (1.f + color / tonemap.white)) / (1.f + color);
	//color = color / (1.f + color);

	color = pow(color, 1 / 2.2f);

	return color;
}