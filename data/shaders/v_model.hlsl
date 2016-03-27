#include "vs_constants.h"

cbuffer PerFrame : register(b0)
{
	VsPerFrame cbPerFrame;
}; 

cbuffer PerObject : register(b1)
{
	float4x4 mtxWorld;
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

	output.position_ws = mul(input.position, mtxWorld);
	output.position = mul(output.position_ws, cbPerFrame.mtxViewProj);

	output.normal = mul(input.normal, (float3x3)mtxWorld);
	output.tangent = mul(input.tangent, (float3x3)mtxWorld);
	output.bitangent = mul(input.bitangent, (float3x3)mtxWorld);

	output.color = input.color;
	output.texcoords = input.texcoords;
	return output;
}