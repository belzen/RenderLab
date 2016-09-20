#include "v_constants.h"
#include "v_output.hlsli"

cbuffer PerAction : register(b0)
{
	VsPerAction cbPerAction;
};

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
	output.position = mul(output.position_ws, cbPerAction.mtxViewProj);

#if !DEPTH_ONLY
	output.texcoords = float2(output.position_ws.x, output.position_ws.z);
	output.normal = float3(0.f, 1.f, 0.f);
#endif

	return output;
}
