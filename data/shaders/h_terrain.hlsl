#include "v_output.hlsli"

#define NUM_CONTROL_POINTS 4

HsPatchConstants CalcHSPatchConstants(
	InputPatch<VsOutputTerrain, NUM_CONTROL_POINTS> input,
	uint patchId : SV_PrimitiveID)
{
	HsPatchConstants patchConstants;

	patchConstants.edgeTessFactor[0] = 15.f;
	patchConstants.edgeTessFactor[1] = 15.f;
	patchConstants.edgeTessFactor[2] = 15.f;
	patchConstants.edgeTessFactor[3] = 15.f;

	patchConstants.insideTessFactors[0] = 15.f;
	patchConstants.insideTessFactors[1] = 15.f;

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

	return output;
}
