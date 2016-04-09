#include "vs_constants.h"
#include "vs_output.hlsli"

cbuffer PerAction : register(b0)
{
	VsPerAction cbPerAction;
}; 

cbuffer PerObject : register(b1)
{
	float4x4 mtxWorld;
};

struct VertexInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
	float3 tangent : TANGENT;
	float3 bitangent : BINORMAL;
};

VsOutputModel main( VertexInput input )
{
	VsOutputModel output = (VsOutputModel)0;

	output.position_ws = mul(float4(input.position,1), mtxWorld);

#if !CUBEMAP_CAPTURE // This is unnecessary if we're using cubemap capture geometry shader.
	output.position = mul(output.position_ws, cbPerAction.mtxViewProj);
#endif

#if !DEPTH_ONLY
	output.normal = mul(input.normal, (float3x3)mtxWorld);
	output.tangent = mul(input.tangent, (float3x3)mtxWorld);
	output.bitangent = mul(input.bitangent, (float3x3)mtxWorld);

	output.color = input.color;
	output.texcoords = input.texcoords;
#endif

	return output;
}