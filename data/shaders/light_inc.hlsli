#include "light_types.h"
#include "atmosphere_inc.hlsli"

// Specular models
#define PHONG 1
#define BLINN_PHONG 2
#define SPECULAR_MODEL BLINN_PHONG // Active specular model

static float3 ambient_color = float3(1.f, 1.f, 1.f);
static float ambient_intensity = 1.f;

SamplerState samClamp : register(s14);
SamplerComparisonState sampShadowMaps : register(s15);

Texture2D texSkyTransmittanceLut : register(t13);
Texture2DArray texShadowMaps : register(t14);
TextureCubeArray texShadowCubemaps : register(t15);

StructuredBuffer<SpotLight> g_spotLights : register(t16);
StructuredBuffer<PointLight> g_pointLights : register(t17);
Buffer<uint> g_lightIndices : register(t18);

StructuredBuffer<ShaderShadowData> g_shadowData : register(t19);


static const float kShadowBias = 0.002f;

float calcShadowFactor(in float3 pos_ws, in float3 light_dir, in float lightRadius, in uint shadowMapIndex)
{
	if (shadowMapIndex >= MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS)
	{
		return 1.f;
	}

	float4 pos = mul(float4(pos_ws, 1), g_shadowData[shadowMapIndex].mtxViewProj);

	// Shift from [-1,1] range to [0,1]
	float2 uv;
	uv.x = (pos.x / pos.w) * 0.5f + 0.5f;
	uv.y = -(pos.y / pos.w) * 0.5f + 0.5f;

	if ((saturate(uv.x) == uv.x) && (saturate(uv.y) == uv.y))
	{
		float lightDepthValue = pos.z / pos.w - kShadowBias;

#if POISSON_SHADOWS
		float vis = 1.f;
		for (int i = 0; i < 4; ++i)
		{
			float res = texShadowMaps.SampleCmpLevelZero(sampShadowMaps, float3(uv.xy + poissonDisk[i] / 700.f, shadowMapIndex), lightDepthValue).x;
			vis -= (1.f - res) * 0.25f;
		}
		return vis;
#else
		// 16-tap PCF
		float sum = 0.f;
		for (float y = -1.5f; y <= 1.5f; y += 1.f)
		{
			for (float x = -1.5f; x <= 1.5f; x += 1.f)
			{
				float2 offset = float2(x, y) / float2(2048.f, 2048.f); // todo: remove hard-coded texture size
				sum += texShadowMaps.SampleCmpLevelZero(sampShadowMaps, float3(uv.xy + offset, shadowMapIndex), lightDepthValue).x;
			}
		}
		return sum / 16.f;
#endif

	}
	else
	{
		return 1.f;
	}
}

float calcShadowFactorCubeMap(in float3 pos_ws, in float3 light_dir, in float3 posToLight, in float lightRadius, in uint shadowMapIndex)
{
	if (shadowMapIndex >= MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS)
	{
		return 1.f;
	}
		
	float3 absPosDiff = abs(posToLight);
	float zDist = max(absPosDiff.x, max(absPosDiff.y, absPosDiff.z));

	float zNear = 0.1f;
	float zFar = lightRadius * 2.f;

	float depthVal = (zFar + zNear) / (zFar - zNear) - (2 * zFar*zNear) / (zFar - zNear) / zDist;
	depthVal = (depthVal + 1.0) * 0.5 - kShadowBias;

	return texShadowCubemaps.SampleCmpLevelZero(sampShadowMaps, float4(-light_dir, shadowMapIndex - MAX_SHADOW_MAPS), depthVal).x;
}

uint getClusterId(in float2 screenPos, in uint2 screenSize, in float depth)
{
	uint clusterX = screenPos.x / CLUSTEREDLIGHTING_TILE_SIZE;
	uint clusterY = screenPos.y / CLUSTEREDLIGHTING_TILE_SIZE;
	uint numClusterX = ceil(screenSize.x / float(CLUSTEREDLIGHTING_TILE_SIZE));
	uint numClusterY = ceil(screenSize.y / float(CLUSTEREDLIGHTING_TILE_SIZE));

	uint clusterZ = 0;
	float maxDepth = CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH;

	[unroll]
	while (clusterZ < CLUSTEREDLIGHTING_DEPTH_SLICES && depth > maxDepth)
	{
		clusterZ++;
		maxDepth = CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH * pow(CLUSTEREDLIGHTING_MAX_DEPTH / CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH, clusterZ / (CLUSTEREDLIGHTING_DEPTH_SLICES - 1.f));
	}

	return clusterX + clusterY * numClusterX + clusterZ * numClusterX * numClusterY;
}

uint getTileId(in float2 screenPos, in uint screenWidth)
{
	uint tileX = screenPos.x / TILEDLIGHTING_TILE_SIZE;
	uint tileY = screenPos.y / TILEDLIGHTING_TILE_SIZE;
	uint numTileX = ceil(screenWidth / float(TILEDLIGHTING_TILE_SIZE));
	return tileX + tileY * numTileX;
}

uint getLightListIndex(in float2 screenPos, in uint2 screenSize, float depth)
{
#if CLUSTERED_LIGHTING
	return getClusterId(screenPos, screenSize, depth) * CLUSTEREDLIGHTING_BLOCK_SIZE;
#else
	return getTileId(screenPos, screenSize.x) * TILEDLIGHTING_BLOCK_SIZE;
#endif
}

void accumulateLighting(
	in float3 normal, in float3 dirToLight, 
	in float3 lightColor, in float3 viewDir, 
	in float shadowFactor, in float falloff,
	inout float3 diffuse, inout float3 specular)
{
	float ndl = saturate(dot(normal, dirToLight));
	diffuse += falloff * shadowFactor * lightColor * ndl;

	const float kSpecExponent = 150.f;
	const float kSpecIntensity = 0.1f;

#if SPECULAR_MODEL == PHONG
	float3 reflectVec = normalize(2 * ndl * normal - dirToLight);
	specular += falloff * shadowFactor * pow(saturate(dot(reflectVec, viewDir)), kSpecExponent) * kSpecIntensity;
#elif SPECULAR_MODEL == BLINN_PHONG
	float3 halfVec = normalize(dirToLight + viewDir);
	specular += falloff * shadowFactor * pow(saturate(dot(normal, halfVec)), kSpecExponent) * kSpecIntensity;
#endif
}

float3 doLighting(in float3 pos_ws, in float3 color, in float3 normal, in float3 viewDir, in float2 screenPos, in uint2 screenSize)
{
	float viewDepth = dot((pos_ws - cbPerAction.cameraPos), cbPerAction.cameraDir);
	float3 litColor = 0;

	int lightListIdx = getLightListIndex(screenPos, screenSize, viewDepth);
	uint numSpotLights = g_lightIndices[lightListIdx++];
	uint numPointLights = g_lightIndices[lightListIdx++];

	uint i;
	float3 diffuse = 0;
	float3 specular = 0;

	/// Directional lights
	for (i = 0; i < cbDirectionalLights.numLights; ++i)
	{
		DirectionalLight light = cbDirectionalLights.lights[i];
		float3 dirToLight = -light.direction;

		// Find actual shadow map index
		int shadowMapIndex = light.shadowMapIndex;
		while (viewDepth > g_shadowData[shadowMapIndex].partitionEndZ)
		{
			++shadowMapIndex;
		}

		// Apply transmittance to the directional light, otherwise surfaces will be way brighter than the sky.
		// Source: Intel's OutdoorLightScattering demo
		// Note: This assumes every directional light is a sun.
		float altitude = length(atmViewPosToPlanetPos(pos_ws));
		float cosViewAngle = dirToLight.y;
		float3 transmittance = atmSampleTransmittance(texSkyTransmittanceLut, samClamp, altitude, cosViewAngle);

		float reflectance = cbAtmosphere.averageGroundReflectance; // todo: What about non-terrain objects?
		float3 sunColor = light.color * transmittance * reflectance / kPi;

		// Calc shadow and apply the attenuated sun light.
		float shadowFactor = calcShadowFactor(pos_ws, dirToLight, 1000000.f, shadowMapIndex);
		accumulateLighting(normal, dirToLight, sunColor, viewDir, shadowFactor, 1.f, diffuse, specular);
	}


	/// Spot lights
	for (i = 0; i < numSpotLights; ++i)
	{
		SpotLight light = g_spotLights[g_lightIndices[lightListIdx++]];

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

		accumulateLighting(normal, dirToLight, light.color, viewDir, shadowFactor, distFalloff * angularFalloff, diffuse, specular);
	}

	/// Point lights
	for (i = 0; i < numPointLights; ++i)
	{
		PointLight light = g_pointLights[g_lightIndices[lightListIdx++]];

		float3 posToLight = light.position - pos_ws;
		float3 dirToLight = normalize(posToLight);

		// Distance falloff
		// todo: constant, linear, quadratic attenuation instead?
		float lightDist = length(posToLight);
		float distFalloff = 1.f / (lightDist * lightDist);
		distFalloff *= saturate(1 - (lightDist / light.radius));

		float shadowFactor = calcShadowFactorCubeMap(pos_ws, dirToLight, posToLight, light.radius, light.shadowMapIndex);

		accumulateLighting(normal, dirToLight, light.color, viewDir, shadowFactor, distFalloff, diffuse, specular);
	}

	litColor = diffuse * color + specular;

#if VISUALIZE_LIGHT_LIST
	uint numLights = numSpotLights + numPointLights;
	if (numLights == 1)
		litColor *= float3(1.f, 0.f, 0.f);
	else if (numLights == 2)
		litColor *= float3(0.f, 1.f, 0.f);
	else if (numLights >= 3)
		litColor *= float3(0.f, 0.f, 1.f);
#endif // VISUALIZE_LIGHT_LIST


	float kEpsilon = 0.0001f; // Small value to prevent solid black color which can mess up tonemapping.
	float3 ambient = color * ambient_color * ambient_intensity;
	return ambient + litColor + kEpsilon;
}
