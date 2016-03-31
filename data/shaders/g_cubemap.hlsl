#include "vs_output.hlsli"
#include "gs_constants.h"

struct GSOutput
{
	// Copied from VSOutput
	/*float4 position : SV_POSITION;
	float4 position_ws : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 texcoords : TEXCOORD0;
	float3 tangent : TANGENT;
	float3 bitangent : BINORMAL;*/
	VSOutput vsOutput;

	uint renderTargetIndex : SV_RenderTargetArrayIndex;
};

cbuffer CubemapPerFrameBuffer
{
	GsCubemapPerFrame cbCubemap;
};

[maxvertexcount(18)]
void main(
	triangle VSOutput input[3],
	inout TriangleStream< GSOutput > output
)
{
	for (uint f = 0; f < 6; f++)
	{
		for (uint i = 0; i < 3; ++i)
		{
			GSOutput cubeVert = (GSOutput)0;
			cubeVert.vsOutput = input[i];
			cubeVert.vsOutput.position = mul(cubeVert.vsOutput.position_ws, cbCubemap.mtxViewProj[f]);
			cubeVert.renderTargetIndex = f;

			output.Append(cubeVert);
		}

		output.RestartStrip();
	}
}