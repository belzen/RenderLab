struct Light
{
	int type;
	float3 position;
	float3 direction;
	float radius;
	float3 color;
	float innerConeAngle; // Angle where light begins to fall off
	float outerConeAngle; // No more light
	uint castsShadows;
};

#define MAX_LIGHTS_PER_TILE 128

#if !LIGHT_DATA_ONLY

StructuredBuffer<Light> g_lights : register(t16);
StructuredBuffer<uint> g_tileLightIndices : register(t17);

int getTileId(in float2 screenPos, in uint screenWidth)
{
	const int kTileSize = 16;
	int tileX = screenPos.x / kTileSize;
	int tileY = screenPos.y / kTileSize;
	int numTileX = screenWidth / kTileSize;
	return tileX + tileY * numTileX;
}

float4 doLighting(in float3 pos_ws, in float4 color, in float3 normal, in float3 viewDir, in float2 screenPos, in uint screenWidth)
{
	int tileIdx = getTileId(screenPos, screenWidth) * MAX_LIGHTS_PER_TILE;
	int numTiles = g_tileLightIndices[tileIdx];

	float4 litColor = 0;

	for (int i = 0; i < numTiles; ++i)
	{
		Light light = g_lights[ g_tileLightIndices[tileIdx + i + 1] ];

		if (light.type == 0) // Directional
		{
			float ndl = saturate(dot(normal, -light.direction));
			float4 diffuse = color * ndl * float4(light.color, 1);

			float3 reflect = normalize(2 * ndl * normal + light.direction);
			float4 specular = pow(saturate(dot(reflect, viewDir)), 8);

			float shadow = saturate(4 * ndl);
			litColor += shadow * (diffuse + specular);
		}
		else if (light.type == 1) // Point
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
			litColor += shadow * (diffuse + specular);
		}
		else if (light.type == 0) // Spot
		{

		}
	}

	return litColor;
}

#endif