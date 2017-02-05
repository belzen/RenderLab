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

// https://en.wikipedia.org/wiki/Dither
// https://en.wikipedia.org/wiki/Ordered_dithering
// This matrix matches the 4x4 example in the first image on the Ordered_dithering wiki
// Note, however, that all values are increased by one.  This is to ensure alpha = 0 always
//	results in a negative value because clip() only occurs if the value is negative.
static const float kDitherDenom = 17.f;
static const float4x4 kDitherMatrix = {
	1.f  / kDitherDenom,  9.f  / kDitherDenom,  3.f  / kDitherDenom,  11.f / kDitherDenom,
	13.f / kDitherDenom,  5.f  / kDitherDenom,  15.f / kDitherDenom,  7.f  / kDitherDenom,
	4.f  / kDitherDenom,  12.f / kDitherDenom,  2.f  / kDitherDenom,  10.f / kDitherDenom,
	16.f / kDitherDenom,  8.f  / kDitherDenom,  14.f / kDitherDenom,  6.f  / kDitherDenom
};

PsOutput main(VsOutputModel input)
{
	float alpha = cbMaterial.color.a;
	#if ALPHA_STIPPLED
		clip(alpha - kDitherMatrix[(uint)input.position.x % 4][(uint)input.position.y % 4]);
	#endif

	#if ALPHA_CUTOUT
		float4 mask = texAlphaMask.Sample(sampAlphaMask, input.texcoords);
		clip(mask.x - 0.5f);
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

		float3 litColor;
		float3 albedo;

		PsOutput output;
		doLighting(input.position_ws.xyz, 
			color.rgb, alpha, cbMaterial.roughness, cbMaterial.metalness, 
			normal, cameraViewDir, input.position.xy, 
			g_texEnvironmentMaps, g_texVolumetricFogLut,
			// outputs
			litColor, albedo);

		output.color.rgb = litColor;
		output.color.a = alpha;
		#if WRITE_GBUFFERS
			output.albedo = albedo;
			output.normal = normal * 0.5f + 0.5f;
		#endif

		return output;
	#endif
}