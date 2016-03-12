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

#define MAX_SHADOWMAPS 10
#define MAX_LIGHTS_PER_TILE 128
#define TILE_SIZE 16.f

#if !LIGHT_DATA_ONLY

StructuredBuffer<Light> g_lights : register(t16);
StructuredBuffer<uint> g_tileLightIndices : register(t17);

StructuredBuffer<ShadowData> g_shadowData : register(t18);
Texture2DArray shadowMaps : register(t15);
SamplerComparisonState shadowMapsSampler : register(s15);

int getTileId(in float2 screenPos, in uint screenWidth)
{
	float tileX = floor(screenPos.x / TILE_SIZE);
	float tileY = floor(screenPos.y / TILE_SIZE);
	float numTileX = ceil(screenWidth / TILE_SIZE);
	return tileX + tileY * numTileX;
}

float getShadowFactor(in float3 pos_ws, in uint shadowMapIndex)
{
	if (shadowMapIndex >= MAX_SHADOWMAPS)
	{
		return 1.f;
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
			float bias = 0.001f;
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

		if (light.type == 0) // Directional
		{
			float ndl = saturate(dot(normal, -light.direction));
			float4 diffuse = color * ndl * float4(light.color, 1);

			float3 reflect = normalize(2 * ndl * normal + light.direction);
			float4 specular = pow(saturate(dot(reflect, viewDir)), 8);

			float shadow = saturate(4 * ndl) * getShadowFactor(pos_ws, light.shadowMapIndex);
			litColor += shadow * (diffuse + specular);
		}
		else // Point & spot light
		{
			float3 lightPos = pos_ws - light.position;
			float lightDistSqr = dot(lightPos, lightPos);
			float falloff = saturate(((light.radius * light.radius) - lightDistSqr) / lightDistSqr);

			float3 lightDir = normalize(lightPos);
			float ndl = saturate(dot(normal, -lightDir)) * falloff;

			float4 diffuse = color * ndl * float4(light.color, 1);

			float3 reflect = normalize(2 * ndl * normal + lightDir);
			float4 specular = pow(saturate(dot(reflect, viewDir)), 8);

			float shadow = saturate(4 * ndl);
			float intensity = 1.f;

			// todo: constant, linear, quadratic attenuation
			if (light.type == 2) // Spot
			{
				float spotEffect = dot(light.direction, lightDir); //angle
				float coneFalloffRange = max((light.innerConeAngleCos - light.outerConeAngleCos), 0.00001f);
				intensity = saturate( (spotEffect - light.outerConeAngleCos) / coneFalloffRange );
				//intensity = spotEffect > light.innerConeAngleCos;
			}

			litColor += intensity * (shadow * (diffuse + specular));
		}
	}

	return litColor;
}

#endif