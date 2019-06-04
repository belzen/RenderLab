#include "p_common.hlsli"
#include "v_output.hlsli"
#include "global_resources.hlsli"
#include "light_inc.hlsli"

PsOutput main(VsOutputWater input)
{
	float4 color = float4(0.0f, 0.5f, 1.0f, 1.0f);

	float3 cameraViewDir = normalize(cbPerAction.cameraPos - input.position_ws.xyz);

	float3 litColor;
	float3 albedo;

	const float kRoughness = 0.f;
	const float kMetalness = 0.f;

	float3 normal = float3(0.f, 1.f, 0.f);

	doLighting(input.position_ws.xyz,
		color.rgb, 1.f, kRoughness, kMetalness,
		normal, cameraViewDir, input.position.xy,
		g_texEnvironmentMaps, g_texVolumetricFogLut,
		// outputs
		litColor, albedo);

	PsOutput output;
	output.color.rgb = litColor;
	output.color.a = 1.f;
	output.albedo = albedo;
	output.normal = normal * 0.5f + 0.5f;
	return output;
}