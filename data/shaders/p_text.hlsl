
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

	float4 color = float4(0,0,0,0);
	if (sdf > 0.5f)
		color = input.color;
	else if (sdf > 0.3f)
		color = float4(0.f, 0.f, 0.f, 1.f);

	return color;
}