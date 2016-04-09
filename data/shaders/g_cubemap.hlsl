#include "vs_output.hlsli"
#include "gs_constants.h"

struct GsOutputModel
{
	VsOutputModel vsOutput;
	uint renderTargetIndex : SV_RenderTargetArrayIndex;
};

cbuffer CubemapPerActionBuffer
{
	GsCubemapPerAction cbCubemap;
};

[maxvertexcount(18)]
void main(
	triangle VsOutputModel input[3],
	inout TriangleStream< GsOutputModel > output
)
{
	for (uint f = 0; f < 6; f++)
	{
		for (uint i = 0; i < 3; ++i)
		{
			GsOutputModel cubeVert = (GsOutputModel)0;
			cubeVert.vsOutput = input[i];
			cubeVert.vsOutput.position = mul(cubeVert.vsOutput.position_ws, cbCubemap.mtxViewProj[f]);
			cubeVert.renderTargetIndex = f;

			output.Append(cubeVert);
		}

		output.RestartStrip();
	}
}