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

// todo: Store Light in constant buffer, it's way faster https://developer.nvidia.com/content/how-about-constant-buffers
struct ShadowData
{
	float4x4 mtxViewProj;
	float partitionEndZ;
};

#define MAX_SHADOW_MAPS 10
#define MAX_SHADOW_CUBEMAPS 2
#define MAX_LIGHTS_PER_TILE 128
#define TILE_SIZE 16.f

#if !LIGHT_DATA_ONLY

static float3 ambient_color = float3(1.f, 1.f, 1.f);
static float ambient_intensity = 1.f;

StructuredBuffer<Light> g_lights : register(t16);
StructuredBuffer<uint> g_tileLightIndices : register(t17);

StructuredBuffer<ShadowData> g_shadowData : register(t18);

Texture2DArray texShadowMaps : register(t14);
TextureCubeArray texShadowCubemaps : register(t15);
SamplerComparisonState sampShadowMaps : register(s15);

int getTileId(in float2 screenPos, in uint screenWidth)
{
	float tileX = floor(screenPos.x / TILE_SIZE);
	float tileY = floor(screenPos.y / TILE_SIZE);
	float numTileX = ceil(screenWidth / TILE_SIZE);
	return tileX + tileY * numTileX;
}

float calcShadowFactor(in float3 pos_ws, in float3 light_dir, in float3 posToLight, in float lightRadius, in uint shadowMapIndex)
{
	const float bias = 0.002f;
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

		return texShadowCubemaps.SampleCmpLevelZero(sampShadowMaps, float4(-light_dir, shadowMapIndex - MAX_SHADOW_MAPS), depthVal).x;
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
				float res = texShadowMaps.SampleCmpLevelZero(sampShadowMaps, float3(uv.xy + poissonDisk[i] / 700.f, shadowMapIndex), lightDepthValue).x;
				vis -= (1.f - res) * 0.2f;
			}
			return vis;
#else
			// 16-tap PCF
			float sum = 0.f;
			for (float y = -1.5f; y < 1.5f; y += 1.f)
			{
				for (float x = -1.5f; x < 1.5f; x += 1.f)
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
}

float3 doLighting(in float3 pos_ws, in float3 color, in float3 normal, in float3 viewDir, in float2 screenPos, in uint screenWidth)
{
	int tileIdx = getTileId(screenPos, screenWidth) * MAX_LIGHTS_PER_TILE;
	int numLights = g_tileLightIndices[tileIdx];

	float3 litColor = 0;
	float viewDepth = dot((pos_ws - cbPerAction.cameraPos), cbPerAction.cameraDir);

	for (int i = 0; i < numLights; ++i)
	{
		Light light = g_lights[ g_tileLightIndices[tileIdx + i + 1] ];

		float3 posToLight = light.position - pos_ws;
		float lightDist = length(posToLight);
		float3 dirToLight = normalize(posToLight);

		// Choose light dir depending on light type.
		dirToLight = lerp(-light.direction, dirToLight, (light.type > 0));

		// --- Lighting model
		float ndl = saturate(dot(normal, dirToLight));
		float3 diffuse = color * ndl * light.color;

#define PHONG 0
#define BLINN_PHONG 1
		const float kSpecExponent = 150.f;
		const float kSpecIntensity = 0.1f;
#if PHONG
		float3 reflectVec = normalize(2 * ndl * normal - dirToLight);
		float4 specular = pow(saturate(dot(reflectVec, viewDir)), kSpecExponent) * kSpecIntensity;
#elif BLINN_PHONG
		float3 halfVec = normalize(dirToLight + viewDir);
		float3 specular = pow(saturate(dot(normal, halfVec)), kSpecExponent) * kSpecIntensity;
#endif
		// ---

		// Point/Spot light distance falloff
		// todo: constant, linear, quadratic attenuation instead?
		float distFalloff = 1.f / (lightDist * lightDist);
		distFalloff *= saturate(1 - (lightDist / light.radius));
		distFalloff = lerp(1.f, distFalloff, (light.type != 0));

		// Spot light angular falloff
		float spotEffect = dot(light.direction, -dirToLight); //angle
		float coneFalloffRange = max((light.innerConeAngleCos - light.outerConeAngleCos), 0.00001f);
		float angularFalloff = saturate( (spotEffect - light.outerConeAngleCos) / coneFalloffRange );
		angularFalloff = lerp(1.f, angularFalloff, (light.type == 2));

		// Find actual shadow map index
		int shadowMapIndex = light.shadowMapIndex;
		if (light.type == 0)
		{
			while (viewDepth > g_shadowData[shadowMapIndex].partitionEndZ)
			{
				++shadowMapIndex;
			}
		}

		float shadowFactor = calcShadowFactor(pos_ws, dirToLight, posToLight, light.radius, shadowMapIndex);

#if VISUALIZE_CASCADES
		if (light.type == 0)
		{
			static const float3 kPartitionColors[4] = {
				float3(0.f, 1.f, 0.f),
				float3(1.f, 1.f, 0.f),
				float3(1.f, 0.f, 0.f),
				float3(0.f, 0.f, 1.f)
			};
			diffuse *= kPartitionColors[shadowMapIndex - light.shadowMapIndex];
			specular *= kPartitionColors[shadowMapIndex - light.shadowMapIndex];
		}
#endif

		litColor += (diffuse + specular) * distFalloff * angularFalloff * shadowFactor;
	}

	float3 ambient = color * ambient_color * ambient_intensity;
	return ambient + litColor;
}

#endif