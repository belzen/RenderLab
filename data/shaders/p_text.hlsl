#include "p_common.hlsli"

struct PixelInput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
};

Texture2D tex;
SamplerState texSampler;

float4 main(PixelInput input) : SV_TARGET
{
	float sdf = tex.Sample(texSampler, input.texcoords).r;

	const float kTighten = 0.02f;
	sdf += kTighten;

	float primaryWeight = (sdf - 0.4f) / 0.075f;
	float4 color = lerp(float4(0.f, 0.f, 0.f, 1.f), input.color, primaryWeight);

	float isInsideShape = saturate((sdf - 0.3f) / 0.1f);
	color.a *= isInsideShape;

	return color;
}