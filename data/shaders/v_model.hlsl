#include "v_constants.h"
#include "v_output.hlsli"

cbuffer PerAction : register(b0)
{
	VsPerAction cbPerAction;
};

#if IS_INSTANCED
StructuredBuffer<VsPerObject> g_bufPerObject : register(t0);

cbuffer ObjectIdsBuffer : register(b1)
{
	uint4 objectIds[512];
}

#else
cbuffer PerObject : register(b1)
{
	VsPerObject cbPerObject;
};
#endif

struct VertexInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
	float3 tangent : TANGENT;
	float3 bitangent : BINORMAL;
#if IS_INSTANCED
	uint instanceId : SV_InstanceID;
#endif
};

VsOutputModel main( VertexInput input )
{
	VsOutputModel output = (VsOutputModel)0;

#if IS_INSTANCED
	uint idx = objectIds[input.instanceId / 4][input.instanceId % 4];
	VsPerObject perObject = g_bufPerObject[idx];
	float4x4 mtxWorld = perObject.mtxWorld;
#else
	float4x4 mtxWorld = cbPerObject.mtxWorld;
#endif

	output.position_ws = mul(float4(input.position, 1), mtxWorld);

	output.position = mul(output.position_ws, cbPerAction.mtxViewProj);

#if !DEPTH_ONLY
	output.normal = mul(input.normal, (float3x3)mtxWorld);
	output.tangent = mul(input.tangent, (float3x3)mtxWorld);
	output.bitangent = mul(input.bitangent, (float3x3)mtxWorld);

	output.color = input.color;
	output.texcoords = input.texcoords;
#elif ALPHA_CUTOUT
	output.texcoords = input.texcoords;
#endif

	return output;
}