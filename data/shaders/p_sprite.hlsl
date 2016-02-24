
struct PixelInput
{
	float4 position : SV_POSITION;
	float2 texcoords : TEXCOORD0;
	float alpha : TEXCOORD1;
};

Texture2D tex;
SamplerState texSampler;

float4 main(PixelInput input) : SV_TARGET
{
	float4 color = tex.Sample(texSampler, input.texcoords);
	color.a *= input.alpha;
	return color;
}