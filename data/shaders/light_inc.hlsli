#include "light_types.h"
#include "atmosphere_inc.hlsli"

// Specular models
#define PHONG 1
#define BLINN_PHONG 2
#define PBR 3
#define SPECULAR_MODEL PBR // Active specular model

static const float3 ambient_color = float3(1.f, 1.f, 1.f);
static const float ambient_intensity = 0.05f;
static const float kShadowBias = 0.002f;

float calcShadowFactor(in float3 pos_ws, in float3 light_dir, in float lightRadius, in uint shadowMapIndex)
{
	float result = 1.f;
	if (shadowMapIndex < MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS)
	{
		float4 pos = mul(float4(pos_ws, 1), cbGlobalLights.shadowData[shadowMapIndex].mtxViewProj);

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
				float res = g_texShadowMaps.SampleCmpLevelZero(g_samShadowMaps, float3(uv.xy + poissonDisk[i] / 700.f, shadowMapIndex), lightDepthValue).x;
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
					sum += g_texShadowMaps.SampleCmpLevelZero(g_samShadowMaps, float3(uv.xy + offset, shadowMapIndex), lightDepthValue).x;
				}
			}
			result = sum / 16.f;
#endif
		}
	}

	return result;
}

float calcShadowFactorCubeMap(in float3 pos_ws, in float3 light_dir, in float3 posToLight, in float lightRadius, in uint shadowMapIndex)
{
	float result = 1.f;
	if (shadowMapIndex < MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS)
	{
		float3 absPosDiff = abs(posToLight);
		float zDist = max(absPosDiff.x, max(absPosDiff.y, absPosDiff.z));

		float zNear = 0.1f;
		float zFar = lightRadius * 2.f;

		float depthVal = (zFar + zNear) / (zFar - zNear) - (2 * zFar*zNear) / (zFar - zNear) / zDist;
		depthVal = (depthVal + 1.0) * 0.5 - kShadowBias;

		result = g_texShadowCubemaps.SampleCmpLevelZero(g_samShadowMaps, float4(-light_dir, shadowMapIndex - MAX_SHADOW_MAPS), depthVal).x;
	}
	return result;
}

uint getClusterId(in float2 screenPos, in float depth)
{
	uint clusterX = screenPos.x / cbGlobalLights.clusterTileSize;
	uint clusterY = screenPos.y / cbGlobalLights.clusterTileSize;
	uint numClusterX = ceil(cbPerAction.viewSize.x / float(cbGlobalLights.clusterTileSize));
	uint numClusterY = ceil(cbPerAction.viewSize.y / float(cbGlobalLights.clusterTileSize));

	uint clusterZ = 0;
	float maxDepth = CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH;

	// TODO2: This is terrible.  Clustered depth slice needs to be resolvable without a loop.
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

uint getLightListIndex(in float2 screenPos, float depth)
{
#if CLUSTERED_LIGHTING
	return getClusterId(screenPos, depth) * CLUSTEREDLIGHTING_BLOCK_SIZE;
#else
	return getTileId(screenPos, cbPerAction.viewSize.x) * TILEDLIGHTING_BLOCK_SIZE;
#endif
}

float specularD(float roughness, float ndh)
{
	float alpha = roughness * roughness;
	float alphaSqr = alpha * alpha;

	float div = ndh * ndh * (alphaSqr - 1) + 1;
	div = max(0.001f, div);
	return alphaSqr / (kPi * div * div);
}

float specularG(float roughness, float ndv, float ndl)
{
	float k = (roughness + 1) * (roughness + 1) / 8;
	float g1v = ndv / (ndv * (1.f - k) + k);
	float g1l = ndl / (ndl * (1.f - k) + k);
	return g1v * g1l;
}

float3 specularF(float3 specColor, float3 viewDir, float3 halfDir)
{
	float vdh = dot(viewDir, halfDir);
	return specColor + (1 - specColor) * exp2((-5.55473f * vdh - 6.98316f) * vdh);
}

void accumulateLighting(
	in float3 normal, in float3 dirToLight, in float3 specColor, in float roughness,
	in float3 lightColor, in float3 viewDir, in float shadowFactor, in float falloff,
	inout float3 diffuse, inout float3 specular)
{
	float ndl = saturate(dot(normal, dirToLight));
	if (ndl > 0)
	{
		// Apply light attenuation
		lightColor *= falloff * shadowFactor;

		// Diffuse lighting
		diffuse += (lightColor * kOneOverPi) * ndl;

		// Specular lighting
	#if SPECULAR_MODEL == PBR
		float3 halfVec = normalize(dirToLight + viewDir);
		float ndh = saturate(dot(normal, halfVec));
		float ndv = saturate(dot(normal, viewDir)) + 0.00001f;

		float specD = specularD(roughness, ndh);
		float specG = specularG(roughness, ndv, ndl);
		float3 specF = specularF(specColor, viewDir, halfVec);
		float3 brdf = (specD * specG * specF) / (4 * ndl * ndv);

		specular += brdf * lightColor;

	#elif SPECULAR_MODEL == PHONG

		const float kSpecExponent = 150.f;
		const float kSpecIntensity = 0.1f;
		float3 reflectVec = normalize(2 * ndl * normal - dirToLight);
		specular += lightColor * pow(saturate(dot(reflectVec, viewDir)), kSpecExponent) * kSpecIntensity;

	#elif SPECULAR_MODEL == BLINN_PHONG

		const float kSpecExponent = 150.f;
		const float kSpecIntensity = 0.1f;
		float3 halfVec = normalize(dirToLight + viewDir);
		specular += lightColor * pow(saturate(dot(normal, halfVec)), kSpecExponent) * kSpecIntensity;

	#endif
	}
}

float3 applyVolumetricFog(in float3 litColor, in float viewDepth, 
	in float2 screenPos, in Texture3D<float4> texVolumetricFogLut)
{
	float3 fogLutCoords = float3(screenPos.xy / cbPerAction.viewSize, viewDepth / (cbPerAction.volumetricFogFarDepth - cbPerAction.cameraNearDist));
	float4 fog = texVolumetricFogLut.SampleLevel(g_samClamp, fogLutCoords, 0);
	return litColor * fog.a + fog.rgb;
}

float3 doLighting(in float3 pos_ws, in float3 baseColor, 
	in float roughness, in float metalness,
	in float3 normal, in float3 viewDir, 
	in float2 screenPos, 
	in TextureCubeArray texEnvironmentMaps, in Texture3D<float4> texVolumetricFogLut)
{
	float viewDepth = dot((pos_ws - cbPerAction.cameraPos), cbPerAction.cameraDir);
	float3 litColor = 0;

	uint lightListIdx = getLightListIndex(screenPos, viewDepth);
	uint numSpotLights = g_bufLightIndices[lightListIdx++];
	uint numPointLights = g_bufLightIndices[lightListIdx++];

	uint i;
	float3 diffuse = 0;
	float3 specular = 0;

	float3 albedo = baseColor - baseColor * metalness;
	float3 dielectricColor = 0.04f;
	float3 specColor = lerp(dielectricColor, baseColor, metalness);

	/// Directional lights
	for (i = 0; i < cbGlobalLights.numDirectionalLights; ++i)
	{
		DirectionalLight light = cbGlobalLights.directionalLights[i];
		float3 dirToLight = -light.direction;

		// Find actual shadow map index
		int shadowMapIndex = light.shadowMapIndex;
		while (viewDepth > cbGlobalLights.shadowData[shadowMapIndex].partitionEndZ)
		{
			++shadowMapIndex;
		}

		// Apply transmittance to the directional light, otherwise surfaces will be way brighter than the sky.
		// Source: Intel's OutdoorLightScattering demo
		// Note: This assumes every directional light is a sun.
		float altitude = length(atmViewPosToPlanetPos(pos_ws)) - cbAtmosphere.planetRadius;
		float cosSunAngle = dirToLight.y;
		float3 transmittance = atmSampleTransmittance(g_texSkyTransmittanceLut, g_samClamp, altitude, cosSunAngle);

		float reflectance = cbAtmosphere.averageGroundReflectance;
		float3 sunColor = light.color * transmittance * reflectance;

		// Calc shadow and apply the attenuated sun light.
		float shadowFactor = calcShadowFactor(pos_ws, dirToLight, 1000000.f, shadowMapIndex);
		accumulateLighting(normal, dirToLight, specColor, roughness, 
			sunColor, viewDir, shadowFactor, 1.f, 
			diffuse, specular);
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
		float distFalloff = (lightDist <= light.radius) / (lightDist * lightDist);

		// Angular falloff
		float spotEffect = dot(light.direction, -dirToLight); //angle
		float coneFalloffRange = max((light.innerConeAngleCos - light.outerConeAngleCos), 0.00001f);
		float angularFalloff = saturate((spotEffect - light.outerConeAngleCos) / coneFalloffRange);

		float shadowFactor = calcShadowFactor(pos_ws, dirToLight, light.radius, light.shadowMapIndex);

		accumulateLighting(normal, dirToLight, specColor, roughness, 
			light.color, viewDir, shadowFactor, distFalloff * angularFalloff, 
			diffuse, specular);
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
		float distFalloff = (lightDist <= light.radius) / (lightDist * lightDist);

		float shadowFactor = calcShadowFactorCubeMap(pos_ws, dirToLight, posToLight, light.radius, light.shadowMapIndex);

		accumulateLighting(normal, dirToLight, specColor, roughness, 
			light.color, viewDir, shadowFactor, distFalloff, 
			diffuse, specular);
	}

	// Reflection
	float3 reflectionVec = reflect(-viewDir, normal);
	float3 reflection = texEnvironmentMaps.Sample(g_samClamp, float4(reflectionVec, cbGlobalLights.globalEnvironmentLight.environmentMapIndex)).rgb;

	litColor = diffuse * albedo + specular + reflection * specColor;
	litColor = applyVolumetricFog(litColor, viewDepth, screenPos, texVolumetricFogLut);

#if VISUALIZE_LIGHT_LIST
	uint numLights = numSpotLights + numPointLights;
	float bucketSize = CLUSTEREDLIGHTING_MAX_LIGHTS_PER / 3;
	if (numLights < bucketSize)
	{
		litColor *= float3(1.f, 0.f, 0.f);
	}
	else if (numLights < bucketSize * 2)
	{
		litColor *= float3(0.f, 1.f, 0.f);
	}
	else
	{
		litColor *= float3(0.f, 0.f, 1.f);
	}
#endif // VISUALIZE_LIGHT_LIST


	float kEpsilon = 0.0001f; // Small value to prevent solid black color which can mess up tonemapping.
	float3 ambient = albedo * ambient_color * ambient_intensity;
	return ambient + litColor + kEpsilon;
}
