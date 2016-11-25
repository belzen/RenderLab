#include "p_common.hlsli"
#include "v_output.hlsli"
#if !DEPTH_ONLY
#include "global_resources.hlsli"
#include "light_inc.hlsli"
#endif

Texture2D texDiffuse : register(t0);
SamplerState sampDiffuse : register(s0);

Texture2D texNormals : register(t1);
SamplerState sampNormals : register(s1);

#if ALPHA_CUTOUT
Texture2D texAlphaMask : register(t2);
SamplerState sampAlphaMask : register(s2);
#endif

PsOutput main(VsOutputModel input)
{
#if ALPHA_CUTOUT
	float4 mask = texAlphaMask.Sample(sampAlphaMask, input.texcoords);
	if (mask.x < 0.5f)
	{
		discard;
	}
#endif

#if !DEPTH_ONLY
	float4 color = texDiffuse.Sample(sampDiffuse, input.texcoords);

	float3 normal;
	normal.xy = (texNormals.Sample(sampNormals, input.texcoords).ga * 2.f) - 1.f; // Expand normal range
	normal.z = normalize(sqrt(1 - normal.x * normal.x - normal.y * normal.y));

	// Transform normal into world space.
	normal = (normal.x * normalize(input.tangent)) + (normal.y * normalize(input.bitangent)) + (normal.z * normalize(input.normal));
	normal = normalize(normal);

	float3 cameraViewDir = normalize(cbPerAction.cameraPos - input.position_ws.xyz);

	PsOutput output;
	output.color.rgb = doLighting(input.position_ws.xyz, 
		color.rgb, cbMaterial.roughness, cbMaterial.metalness, 
		normal, cameraViewDir, input.position.xy, 
		g_texEnvironmentMaps, g_texVolumetricFogLut);
	output.color.a = 1.f;
	return output;
#endif
}