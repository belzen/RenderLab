#include "p_constants.h"
#include "v_output.hlsli"
#include "light_types.h"

cbuffer SsaoParamsBuffer : register(c0)
{
	SsaoParams cbSsaoParams;
};

Texture2D<float> g_texOcclusion : register(t0);
Texture2D<float4> g_texAlbedo : register(t1);

SamplerState g_samClamp : register(s0);

float4 main(VsOutputSprite input) : SV_TARGET
{
	float4 albedo = g_texAlbedo.SampleLevel(g_samClamp, input.texcoords, 0);
	float occlusion = g_texOcclusion.SampleLevel(g_samClamp, input.texcoords, 0);

	// Shader should be setup with subtractive blend mode, 
	// so return the amount of ambient to remove.
	float3 ambientToRemove = (1.f - occlusion) * albedo.rgb * ambient_color * ambient_intensity;
	return float4(ambientToRemove, 1.f);
}