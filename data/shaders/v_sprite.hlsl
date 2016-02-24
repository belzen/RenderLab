cbuffer PerFrame
{
	float4x4 view_mat;
	float4x4 proj_mat;
};

cbuffer PerObject
{
	float3 screen_pos;
	float2 scale;
	float alpha;
};

struct VertexInput
{
	float2 position : POSITION;
	float2 texcoords : TEXCOORD0;
};

struct VertexOutput
{
	float4 position : SV_POSITION;
	float2 texcoords : TEXCOORD0;
	float alpha : TEXCOORD1;
};

VertexOutput main(VertexInput input)
{
	VertexOutput output;

	float4 pos = float4(input.position * scale.xy + screen_pos.xy, screen_pos.z, 1.f);
	output.position = mul(pos, proj_mat);

	output.texcoords = input.texcoords;
	output.alpha = alpha;
	return output;
}