#include "p_common.hlsli"
#include "v_output.hlsli"

float4 main(VsOutputModel input) : SV_TARGET
{
	float ndl = saturate(dot(input.normal, -cbPerAction.cameraDir));
	ndl = max(ndl, 0.5f);
	return float4(ndl * cbMaterial.color, 1);
}
