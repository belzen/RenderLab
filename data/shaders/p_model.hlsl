#include "p_common.hlsli"
#include "light_inc.hlsli"
#include "v_output.hlsli"

Texture2D texDiffuse : register(t0);
SamplerState sampDiffuse : register(s0);

Texture2D texNormals : register(t1);
SamplerState sampNormals : register(s1);

#if ALPHA_CUTOUT
Texture2D texAlphaMask : register(t2);
SamplerState sampAlphaMask : register(s2);
#endif


#if DEPTH_ONLY
void main(VsOutputModel input)
#else
float4 main(VsOutputModel input) : SV_TARGET
#endif
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

	float3 normal = texNormals.Sample(sampNormals, input.texcoords).xyz;
	normal = (normal * 2.f) - 1.f; // Expand normal range

	// Transform normal into world space.
	normal = (normal.x * input.tangent) + (normal.y * input.bitangent) + (normal.z * input.normal);
	normal = normalize(normal);

	float3 cameraViewDir = normalize(cbPerAction.cameraPos - input.position_ws.xyz);

	float3 litColor = doLighting(input.position_ws.xyz, color.rgb, normal, cameraViewDir, input.position.xy, cbPerAction.viewWidth);
	return float4(litColor + 0.001f, 1);
#endif
}