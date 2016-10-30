#include "c_constants.h"
#include "p_constants.h"
#include "light_types.h"

cbuffer PerAction : register(b0)
{
	PsPerAction cbPerAction;
};

cbuffer DirectionalLightsBuffer : register(b1)
{
	DirectionalLightList cbDirectionalLights;
}

cbuffer AtmosphereParamsBuffer : register(b2)
{
	AtmosphereParams cbAtmosphere;
}

cbuffer VolumetricFogBuffer : register(b3)
{
	VolumetricFogParams cbFog;
}

RWTexture3D<float4> g_texFogLightingLut : register(u0);

SamplerState           g_samClamp         : register(s0);
SamplerComparisonState g_samShadowMaps    : register(s1);

Texture2D        g_texSkyTransmittanceLut : register(t0);
Texture2DArray   g_texShadowMaps          : register(t1);
TextureCubeArray g_texShadowCubemaps      : register(t2);

StructuredBuffer<SpotLight>        g_bufSpotLights   : register(t3);
StructuredBuffer<PointLight>       g_bufPointLights  : register(t4);
Buffer<uint>                       g_bufLightIndices : register(t5);
StructuredBuffer<ShaderShadowData> g_bufShadowData   : register(t6);

#include "light_inc.hlsli"

void accumulateDiffuseLighting(
	in float3 lightColor, in float shadowFactor, in float falloff, 
	inout float3 diffuse)
{
}

float3 getWorldPosFromLutCoord(uint3 lutCoord, float viewDepth)
{
	// Calculate frustum plane half-size at this view depth
	float2 planeSize;
	planeSize.y = viewDepth * tan(cbPerAction.cameraFovY * 0.5f);
	planeSize.x = planeSize.y * cbPerAction.aspectRatio;

	// Determine offset and step size between LUT coords at this depth.
	float2 step = 2 * planeSize / cbFog.lutSize.xy;
	step.y *= -1.f;
	float2 offset = step * 0.5f;

	// Starting from the center of the plane, offset to the desired position
	float3 pos_ws = cbPerAction.cameraPos + cbPerAction.cameraDir * viewDepth;
	float3 right = normalize(cross(float3(0, 1, 0), cbPerAction.cameraDir));
	float3 up = normalize(cross(cbPerAction.cameraDir, right));

	float2 screenPos = float2(-planeSize.x, planeSize.y) + offset + step * lutCoord.xy;
	pos_ws += right * screenPos.x + up * screenPos.y;
	return pos_ws;
}

float calcPhase(float cosViewAngle, float phaseG)
{
	float g2 = phaseG * phaseG;
	float num = 3.f * (1 - g2) * (1 + cosViewAngle * cosViewAngle);
	float div = (8.f * kPi) * (2 + g2) * pow(1 + g2 - 2 * phaseG * cosViewAngle, 1.5f);
	return num / div;
}

float calcDensity(float depth)
{
	return 1.f; // todo
}

[numthreads(VOLFOG_LUT_THREADS_X, VOLFOG_LUT_THREADS_Y, 1)]
void main( uint3 globalId : SV_DispatchThreadID )
{
	float3 inscatter = 0.f;
	float depthStep = (cbFog.farDepth - cbPerAction.cameraNearDist) / cbFog.lutSize.z;

	// TODO: Ensure this is aligned with clustered lighting froxels
	float centerDepth = depthStep * (globalId.z + 0.5f);
	float2 screenUv = ((globalId.xy + 0.5f) / cbFog.lutSize.xy);
	uint lightListIdx = getLightListIndex(screenUv * cbPerAction.viewSize, centerDepth);
	uint numSpotLights = g_bufLightIndices[lightListIdx++];
	uint numPointLights = g_bufLightIndices[lightListIdx++];

	// Participating media properties
	// TODO: Depth fog, height fog, fog volumes
	float avgPhaseG = cbFog.phaseG;

	// Iterate through the froxel along the view ray and accumulate average scattered lighting.
	// TODO: Experiment with jittered/sequenced sampling instead of stepping along the ray in the center of the froxel. 
	// TODO: Optimize this loop.  Most of the per-light data can be calculated once,
	//		it isn't going to change much within a single froxel.
	const uint kNumSteps = 8;
	const float kStepDiv = 1.f / kNumSteps;
	uint cachedLightListIdx = lightListIdx;
	for (uint x = 0; x < kNumSteps; ++x)
	{
		float viewDepth = depthStep * (globalId.z + x * kStepDiv);
		float3 pos_ws = getWorldPosFromLutCoord(globalId, viewDepth);
		lightListIdx = cachedLightListIdx;

		// Accumulate lighting
		for (uint i = 0; i < cbDirectionalLights.numLights; ++i)
		{
			DirectionalLight light = cbDirectionalLights.lights[i];
			float3 dirToLight = -light.direction;

			// Find actual shadow map index
			int shadowMapIndex = light.shadowMapIndex;
			while (viewDepth > g_bufShadowData[shadowMapIndex].partitionEndZ)
			{
				++shadowMapIndex;
			}

			// Apply transmittance to the directional light, otherwise surfaces will be way brighter than the sky.
			// Source: Intel's OutdoorLightScattering demo
			// Note: This assumes every directional light is a sun.
			float altitude = length(atmViewPosToPlanetPos(pos_ws)) - cbAtmosphere.planetRadius;
			float cosSunAngle = dirToLight.y;
			float3 transmittance = atmSampleTransmittance(g_texSkyTransmittanceLut, g_samClamp, altitude, cosSunAngle);

			// Calc shadow and apply the attenuated light.
			float shadowFactor = calcShadowFactor(pos_ws, dirToLight, 1000000.f, shadowMapIndex);
			float3 lightDiffuse = shadowFactor * light.color * transmittance;

			float cosViewToSunAngle = dot(cbPerAction.cameraDir, dirToLight);
			inscatter += lightDiffuse * calcPhase(cosViewToSunAngle, avgPhaseG) * calcDensity(viewDepth);
		}

		/// Spot lights
		for (i = 0; i < numSpotLights; ++i)
		{
			SpotLight light = g_bufSpotLights[g_bufLightIndices[lightListIdx++]];

			float3 posToLight = light.position - pos_ws;
			float3 dirToLight = normalize(posToLight);

			// Distance falloff
			// todo: constant, linear, quadratic attenuation instead?
			float lightDist = length(posToLight);
			float distFalloff = 1.f / (lightDist * lightDist);
			distFalloff *= saturate(1 - (lightDist / light.radius));

			// Angular falloff
			float spotEffect = dot(light.direction, -dirToLight); //angle
			float coneFalloffRange = max((light.innerConeAngleCos - light.outerConeAngleCos), 0.00001f);
			float angularFalloff = saturate((spotEffect - light.outerConeAngleCos) / coneFalloffRange);

			float shadowFactor = calcShadowFactor(pos_ws, dirToLight, light.radius, light.shadowMapIndex);
			float3 lightDiffuse = light.color * distFalloff * angularFalloff * shadowFactor;
			float cosViewToLightAngle = dot(cbPerAction.cameraDir, dirToLight);
			inscatter += lightDiffuse * calcPhase(cosViewToLightAngle, avgPhaseG) * calcDensity(viewDepth);
		}

		/// Point lights
		for (i = 0; i < numPointLights; ++i)
		{
			PointLight light = g_bufPointLights[g_bufLightIndices[lightListIdx++]];

			float3 posToLight = light.position - pos_ws;
			float3 dirToLight = normalize(posToLight);

			// Distance falloff
			// todo: constant, linear, quadratic attenuation instead?
			float lightDist = length(posToLight);
			float distFalloff = 1.f / (lightDist * lightDist);
			distFalloff *= saturate(1 - (lightDist / light.radius));

			float shadowFactor = calcShadowFactorCubeMap(pos_ws, dirToLight, posToLight, light.radius, light.shadowMapIndex);
			float3 lightDiffuse = light.color * distFalloff * shadowFactor;
			float cosViewToLightAngle = dot(cbPerAction.cameraDir, dirToLight);
			inscatter += lightDiffuse * calcPhase(cosViewToLightAngle, avgPhaseG) * calcDensity(viewDepth);
		}
	}

	inscatter *= cbFog.scatteringCoeff * kStepDiv;
	float3 extinction = cbFog.scatteringCoeff + cbFog.absorptionCoeff;
	float maxExtinction = max(extinction.r, max(extinction.g, extinction.b));

	g_texFogLightingLut[globalId] = float4(inscatter, maxExtinction);
}
