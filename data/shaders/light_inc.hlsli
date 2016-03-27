struct Light
{
	int type;
	float3 position;
	float3 direction;
	float radius;
	float3 color;
	float innerConeAngleCos; // Cosine of angle where light begins to fall off
	float outerConeAngleCos; // No more light
	uint castsShadows;
	uint shadowMapIndex;
};

struct ShadowData
{
	float4x4 mtxViewProj;
};

#define MAX_SHADOW_MAPS 10
#define MAX_SHADOW_CUBEMAPS 2
#define MAX_LIGHTS_PER_TILE 128
#define TILE_SIZE 16.f

#if !LIGHT_DATA_ONLY

StructuredBuffer<Light> g_lights : register(t16);
StructuredBuffer<uint> g_tileLightIndices : register(t17);

StructuredBuffer<ShadowData> g_shadowData : register(t18);

Texture2DArray shadowMaps : register(t14);
TextureCubeArray shadowCubeMaps : register(t15);
SamplerComparisonState shadowMapsSampler : register(s15);

int getTileId(in float2 screenPos, in uint screenWidth)
{
	float tileX = floor(screenPos.x / TILE_SIZE);
	float tileY = floor(screenPos.y / TILE_SIZE);
	float numTileX = ceil(screenWidth / TILE_SIZE);
	return tileX + tileY * numTileX;
}

float calcShadowFactor(in float3 pos_ws, in float3 light_dir, in float3 posToLight, in float lightRadius, in uint shadowMapIndex)
{
	const float bias = 0.0004f;
	if (shadowMapIndex >= MAX_SHADOW_MAPS + MAX_SHADOW_CUBEMAPS)
	{
		return 1.f;
	}
	else if (shadowMapIndex >= MAX_SHADOW_MAPS)
	{
		float3 absPosDiff = abs(posToLight);
		float zDist = max(absPosDiff.x, max(absPosDiff.y, absPosDiff.z));

		float zNear = 0.1f;
		float zFar = lightRadius * 2.f;

		float depthVal = (zFar + zNear) / (zFar - zNear) - (2 * zFar*zNear) / (zFar - zNear) / zDist;
		depthVal = (depthVal + 1.0) * 0.5 - bias;

		return shadowCubeMaps.SampleCmpLevelZero(shadowMapsSampler, float4(-light_dir, shadowMapIndex - MAX_SHADOW_MAPS), depthVal).x;
	}
	else
	{
		float4 pos = mul(float4(pos_ws, 1), g_shadowData[shadowMapIndex].mtxViewProj);

		// Shift from [-1,1] range to [0,1]
		float2 uv;
		uv.x = (pos.x / pos.w) * 0.5f + 0.5f;
		uv.y = -(pos.y / pos.w) * 0.5f + 0.5f;
		
		if ((saturate(uv.x) == uv.x) && (saturate(uv.y) == uv.y))
		{
			float lightDepthValue = pos.z / pos.w - bias;

#if POISSON_SHADOWS
			float vis = 1.f;
			for (int i = 0; i < 4; ++i)
			{
				float res = shadowMaps.SampleCmpLevelZero(shadowMapsSampler, float3(uv.xy + poissonDisk[i] / 700.f, shadowMapIndex), lightDepthValue).x;
				vis -= (1.f - res) * 0.2f;
			}
			return vis;
#else
			return shadowMaps.SampleCmpLevelZero(shadowMapsSampler, float3(uv.xy, shadowMapIndex), lightDepthValue).x;
#endif

		}
		else
		{
			return 1.f;
		}
	}
}

float4 doLighting(in float3 pos_ws, in float4 color, in float3 normal, in float3 viewDir, in float2 screenPos, in uint screenWidth)
{
	int tileIdx = getTileId(screenPos, screenWidth) * MAX_LIGHTS_PER_TILE;
	int numLights = g_tileLightIndices[tileIdx];

	float4 litColor = 0;

	for (int i = 0; i < numLights; ++i)
	{
		Light light = g_lights[ g_tileLightIndices[tileIdx + i + 1] ];

		float3 posToLight = light.position - pos_ws;
		float lightDistSqr = dot(posToLight, posToLight);
		float3 lightDir = normalize(posToLight); // Direction to light.

		// Choose light dir depending on light type.
		lightDir = lerp(-light.direction, lightDir, (light.type > 0));

		// --- Lighting model
#define PHONG 0
#define BLINN_PHONG 1
		const float kSpecExponent = 50.f;
		const float kSpecIntensity = 0.5f;
#if PHONG
		float ndl = saturate(dot(normal, lightDir));
		float4 diffuse = color * ndl * float4(light.color, 1);

		float3 reflect = normalize(2 * ndl * normal - lightDir);
		float4 specular = pow(saturate(dot(reflect, viewDir)), kSpecExponent) * kSpecIntensity;
#elif BLINN_PHONG
		float ndl = saturate(dot(normal, lightDir));
		float4 diffuse = color * ndl * float4(light.color, 1);

		float3 halfVec = normalize(lightDir + viewDir);
		float4 specular = pow(saturate(dot(halfVec, normal)), kSpecExponent) * kSpecIntensity;
#endif
		// ---

		// Point/Spot light distance falloff
		float distFalloff = saturate(((light.radius * light.radius) - lightDistSqr) / lightDistSqr);
		distFalloff = lerp(1.f, distFalloff, (light.type != 0));

		// Spot light angular falloff
		float spotEffect = dot(light.direction, -lightDir); //angle
		float coneFalloffRange = max((light.innerConeAngleCos - light.outerConeAngleCos), 0.00001f);
		float angularFalloff = saturate( (spotEffect - light.outerConeAngleCos) / coneFalloffRange );
		angularFalloff = lerp(1.f, angularFalloff, (light.type == 2));

		// todo: constant, linear, quadratic attenuation
		litColor += (diffuse + specular) * distFalloff * angularFalloff * calcShadowFactor(pos_ws, lightDir, posToLight, light.radius, light.shadowMapIndex);
	}

	return litColor;
}

#endif