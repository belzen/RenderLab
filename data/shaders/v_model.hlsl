cbuffer PerFrame
{
	float4x4 viewproj_mat;
}; 

cbuffer PerObject
{
	float4x4 world_mat;
};

struct VertexInput
{
	float4 position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
	float3 tangent : TANGENT;
	float3 bitangent : BINORMAL;
};

struct VertexOutput
{
	float4 position : SV_POSITION;
	float4 position_ws : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
	float3 tangent : TANGENT;
	float3 bitangent : BINORMAL;
};

VertexOutput main( VertexInput input )
{
	VertexOutput output;

	output.position_ws = mul(input.position, world_mat);
	output.position = mul(output.position_ws, viewproj_mat);

	output.normal = mul(input.normal, (float3x3)world_mat);
	output.tangent = mul(input.tangent, (float3x3)world_mat);
	output.bitangent = mul(input.bitangent, (float3x3)world_mat);

	output.color = input.color;
	output.texcoords = input.texcoords;
	return output;
}