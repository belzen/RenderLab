#include "v_constants.h"
#include "v_output.hlsli"
#include "d_constants.h"

Texture2D texHeightmap : register(t0);
SamplerState sampHeightmap : register(s0);

cbuffer PerAction : register(b0)
{
	VsPerAction cbPerAction;
}

cbuffer TerrainBuffer : register(b1)
{
	DsTerrain cbTerrain;
}

#define NUM_CONTROL_POINTS 4

[domain("quad")]
DsOutputTerrain main(
	float2 uv : SV_DomainLocation,
	HsPatchConstants patchConstants,
	const OutputPatch<HsOutputTerrain, NUM_CONTROL_POINTS> patch)
{
	DsOutputTerrain output;

	float4 edge1 = lerp(patch[0].position_ws, patch[1].position_ws, uv.x);
	float4 edge2 = lerp(patch[3].position_ws, patch[2].position_ws, uv.x);
	output.position_ws = lerp(edge1, edge2, uv.y);

	float2 uvHeight = (output.position_ws.xz - cbTerrain.lods[patch[0].lod].minPos) / cbTerrain.lods[patch[0].lod].size;
	output.position_ws.y = texHeightmap.SampleLevel(sampHeightmap, uvHeight, 0).r * cbTerrain.heightScale;

	output.position = mul(output.position_ws, cbPerAction.mtxViewProj);

#if !DEPTH_ONLY
	output.texcoords = float2(output.position_ws.x, output.position_ws.z);

	// Calculate normal from height data.
	float2 texelSize = cbTerrain.heightmapTexelSize;
	float4 heights;
	heights.x = texHeightmap.SampleLevel(sampHeightmap, uvHeight + float2(-texelSize.x, 0.f), 0).r;
	heights.y = texHeightmap.SampleLevel(sampHeightmap, uvHeight + float2(texelSize.x, 0.f), 0).r;
	heights.z = texHeightmap.SampleLevel(sampHeightmap, uvHeight + float2(0.f, -texelSize.y), 0).r;
	heights.w = texHeightmap.SampleLevel(sampHeightmap, uvHeight + float2(0.f, texelSize.y), 0).r;
	heights *= cbTerrain.heightScale;

	output.normal = normalize(float3(heights.x - heights.y, 2.f, heights.z - heights.w));
#endif

	return output;
}
