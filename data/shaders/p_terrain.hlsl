#include "p_common.hlsli"
#include "v_output.hlsli"
#if !DEPTH_ONLY
#include "global_resources.hlsli"
#include "light_inc.hlsli"
#endif

PsOutput main(DsOutputTerrain input)
{
#if !DEPTH_ONLY
	float4 color = float4(1.0f, 1.0f, 1.0f, 1.0f);

	float3 cameraViewDir = normalize(cbPerAction.cameraPos - input.position_ws.xyz);

	PsOutput output;
	output.color.rgb = doLighting(input.position_ws.xyz, 
		color.rgb, cbMaterial.roughness, cbMaterial.metalness,
		input.normal, cameraViewDir, input.position.xy, 
		g_texEnvironmentMaps, g_texVolumetricFogLut);
	output.color.a = 1.f;
	return output;
#endif
}