#include "p_constants.h"
#include "v_output.hlsli"
#include "p_util.hlsli"

cbuffer PerAction : register(b0)
{
	PsPerAction cbPerAction;
};

cbuffer SsaoParamsBuffer : register(b1)
{
	SsaoParams cbSsaoParams;
};

Texture2D<float> g_texDepth : register(t0);
Texture2D<float3> g_texNormals : register(t1);
Texture2D<float2> g_texNoise : register(t2);

SamplerState g_samClamp : register(s0);
SamplerState g_samWrap : register(s1);

float3 extractViewPosFromDepth(float2 uvDepth)
{
	// Convert from NDC [-1, 1] space to view space and build ray passing through the pixel
	float ndcX = 2.f * uvDepth.x - 1.f;
	float ndcY = 1.f - 2.f * uvDepth.y;

	float3 viewRay;
	viewRay.x = ndcX * cbPerAction.cameraNearDist / cbPerAction.mtxProj._11;
	viewRay.y = ndcY * cbPerAction.cameraNearDist / cbPerAction.mtxProj._22;
	viewRay.z = cbPerAction.cameraNearDist;
	viewRay = normalize(viewRay);

	// Convert depth buffer to linear depth
	float depth = g_texDepth.SampleLevel(g_samClamp, uvDepth, 0);
	float linearDepth = reconstructViewDepth(depth, cbPerAction.cameraNearDist, cbPerAction.cameraFarDist);

	// Calculate view-space position
	float3 positionView = viewRay * (linearDepth / viewRay.z);
	return positionView;
}

float main(VsOutputSprite input) : SV_TARGET
{
	// Get view position from the pixel.
	float3 viewPosition = extractViewPosFromDepth(input.texcoords);

	// Get pixel normal and transform into view space.
	float3 viewNormal = g_texNormals.SampleLevel(g_samClamp, input.texcoords, 0) * 2.f - 1.f;
	viewNormal = normalize(viewNormal);
	viewNormal = mul(float4(viewNormal, 0), cbPerAction.mtxView).xyz;

	// Sample and decompress tiled noise
	float3 noise;
	noise.xy = g_texNoise.SampleLevel(g_samWrap, input.texcoords * cbSsaoParams.noiseUvScale, 0) * 2.f - 1.f;
	noise.z = 0.f;

	// Build sample kernel rotation matrix.
	float3 tangent = normalize(noise - viewNormal * dot(noise, viewNormal));
	float3 bitangent = cross(viewNormal, tangent);
	float3x3 mtxRotation = float3x3(tangent, bitangent, viewNormal);

	float occlusion = 0.f;

	const int kNumSamples = SSAO_KERNEL_SIZE * SSAO_KERNEL_SIZE;
	for (int i = 0; i < kNumSamples; ++i)
	{
		// Pick sample point
		float3 sampleViewPosition = viewPosition + mul(cbSsaoParams.sampleKernel[i].xyz, mtxRotation) * cbSsaoParams.sampleRadius;

		// Transform into clip space
		float4 sampleClipPosition = mul(float4(sampleViewPosition, 1.f), cbPerAction.mtxProj);

		// Normalize XY coordinates
		float2 sampleDepthUv = sampleClipPosition.xy / sampleClipPosition.w;
		// Compress from [-1,1] to [0,1] for tex coords
		sampleDepthUv = sampleDepthUv * 0.5f + 0.5f;
		// Invert Y to switch from NDC to UVs
		sampleDepthUv.y = 1.f - sampleDepthUv.y;

		float sampleDepth = extractViewPosFromDepth(sampleDepthUv).z;

		// If the actual depth at the pixel is less than the expected sample depth, then it is occluded.
		float occluded = (sampleDepth <= sampleViewPosition.z) ? 1.f : 0.f;
		// Ignore occlusion if it is greater than the sample radius.  It's too far away to be sure it's actually occluding.
		float rangeClamp = (abs(viewPosition.z - sampleDepth) < cbSsaoParams.sampleRadius) ? 1.f : 0.f;

		// Accumulate occlusion
		occlusion += occluded * rangeClamp;
	}

	// Normalize and invert occlusion.  The result can be multiplied directly against ambient lighting.
	return 1.f - (occlusion / kNumSamples);
}