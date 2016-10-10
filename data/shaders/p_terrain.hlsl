#include "p_common.hlsli"
#include "v_output.hlsli"
#if !DEPTH_ONLY
#include "light_inc.hlsli"
#endif

PsOutput main(DsOutputTerrain input)
{
#if !DEPTH_ONLY
	float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);

	float3 cameraViewDir = normalize(cbPerAction.cameraPos - input.position_ws.xyz);

	PsOutput output;
	output.color.rgb = doLighting(input.position_ws.xyz, color.rgb, input.normal, cameraViewDir, input.position.xy, cbPerAction.viewSize);
	output.color.a = 1.f;
	return output;
#endif
}