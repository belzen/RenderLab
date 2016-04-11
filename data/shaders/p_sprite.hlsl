#include "v_output.hlsli"
#include "p_common.hlsli"

Texture2D texSprite : register(t0);
SamplerState sampSprite : register(s0);

float4 main(VsOutputSprite input) : SV_TARGET
{
	float4 color = texSprite.Sample(sampSprite, input.texcoords);
	color.a *= input.alpha;
	return color;
}