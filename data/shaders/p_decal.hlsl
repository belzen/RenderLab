#include "global_resources.hlsli"
#include "p_constants.h"
#include "p_util.hlsli"
#include "v_output.hlsli"

cbuffer PerAction : register(b0)
{
	PsPerAction cbPerAction;
};

cbuffer MaterialParamsBuffer : register(b3)
{
	DecalMaterialParams cbMaterial;
}

Texture2D<float4> g_texColor : register(t0);

// TODO: https://bartwronski.com/2015/03/12/fixing-screen-space-deferred-decals/

float4 main(VsOutputDecal input) : SV_TARGET
{
	float2 uvDepth = (input.position_cs.xy / input.position_cs.w) * 0.5f + 0.5f;
	uvDepth.y = 1.f - uvDepth.y; // Flip Y coordinate.  NDC and texcoords are inverted.

	float depth = g_texScreenDepth.SampleLevel(g_samClamp, uvDepth, 0.f);
	float linearDepth = reconstructViewDepth(depth, cbPerAction.cameraNearDist, cbPerAction.cameraFarDist);

	float3 viewRay = normalize(input.position_vs).xyz;
	float3 positionView = viewRay * (linearDepth / viewRay.z);

	float4 positionWorld = mul(float4(positionView, 1.f), cbPerAction.mtxInvView);
	float4 positionLocal = mul(positionWorld, cbMaterial.mtxInvWorld);

	// Local space is a 1x1x1 cube.
	clip(1.f - abs(positionLocal.xyz));

	float2 texcoords = positionLocal.xz * 0.5f + 0.5f;
	float4 color = g_texColor.Sample(g_samClamp, texcoords);

	return color;
}