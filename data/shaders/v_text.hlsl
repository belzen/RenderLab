cbuffer PerFrame
{
	float4x4 viewproj_mat;
};

cbuffer PerObject
{
	float4 color;
	float3 screen_pos;
	float size;
};

struct VertexInput
{
	float2 position : POSITION;
	float2 texcoords : TEXCOORD0;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
};

VSOutput main( VertexInput input )
{
	VSOutput output;

	float4 pos = float4(input.position * size + screen_pos.xy, screen_pos.z, 1.f);
	output.position = mul(pos, viewproj_mat);

	output.color = color;
	output.texcoords = input.texcoords;
	return output;
}