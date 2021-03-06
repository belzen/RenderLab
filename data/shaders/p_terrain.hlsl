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

	float3 litColor;
	float3 albedo;

	PsOutput output;
	doLighting(input.position_ws.xyz, 
		color.rgb, 1.f, cbMaterial.roughness, cbMaterial.metalness,
		input.normal, cameraViewDir, input.position.xy, 
		g_texEnvironmentMaps, g_texVolumetricFogLut,
		// outputs
		litColor, albedo);

	output.color.rgb = litColor;
	output.color.a = 1.f;
	output.albedo = float4(albedo, 0);
	output.normal = float4(input.normal * 0.5f + 0.5f, 0);
	return output;
#endif
}