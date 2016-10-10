#include "v_output.hlsli"

#define NUM_CONTROL_POINTS 4

HsPatchConstants CalcHSPatchConstants(
	InputPatch<VsOutputTerrain, NUM_CONTROL_POINTS> input,
	uint patchId : SV_PrimitiveID)
{
	HsPatchConstants patchConstants;

	const float kTessFactor = 16.f;
	patchConstants.edgeTessFactor[0] = kTessFactor;
	patchConstants.edgeTessFactor[1] = kTessFactor;
	patchConstants.edgeTessFactor[2] = kTessFactor;
	patchConstants.edgeTessFactor[3] = kTessFactor;

	patchConstants.insideTessFactors[0] = kTessFactor;
	patchConstants.insideTessFactors[1] = kTessFactor;

	return patchConstants;
}

[domain("quad")]
[partitioning("fractional_odd")]
[outputtopology("triangle_cw")]
[outputcontrolpoints(NUM_CONTROL_POINTS)]
[patchconstantfunc("CalcHSPatchConstants")]
HsOutputTerrain main(
	InputPatch<VsOutputTerrain, NUM_CONTROL_POINTS> input, 
	uint pointId : SV_OutputControlPointID,
	uint patchId : SV_PrimitiveID )
{
	HsOutputTerrain output;

	output.position_ws = input[pointId].position_ws;
	output.lod = input[pointId].lod;

	return output;
}
