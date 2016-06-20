#include "light_types.h"

#define THREAD_COUNT_X 8
#define THREAD_COUNT_Y 8
#define THREAD_COUNT_TOTAL (THREAD_COUNT_X * THREAD_COUNT_Y)

cbuffer CullingParams : register(b0)
{
	TiledLightCullingParams cbCullParams;
};

StructuredBuffer<SpotLight> g_spotLights : register(t0);
StructuredBuffer<PointLight> g_pointLights : register(t1);
Texture2D<float2> g_depthMinMax : register(t2);

RWBuffer<uint> g_tileLightIndices : register(u0);

struct Frustum
{
	float4 planes[6];
};

float4 planeFromPoints(float3 pt1, float3 pt2, float3 pt3)
{
	float3 v21 = pt1 - pt2;
	float3 v31 = pt1 - pt3;

	float3 n = normalize( cross(v21, v31) );

	float d = dot(n, pt1);
	return float4(n.xyz, -d);
}

void constructFrustum(uint2 tileId, float2 zMinMax, out Frustum frustum)
{
	// Get the non-rounded number of tiles on the screen.  
	// This is necessary for screens that are not divisible by the tile size.
	// We're converting from NDC space to view space, so we need the actual visible tile on the screen, not the rounded number.
	float2 actualScreenTiles = cbCullParams.screenSize / TILEDLIGHTING_TILE_SIZE;

	// Convert from NDC [-1, 1] space to view space.
	float nearMinX = -(1 - 2.0f / actualScreenTiles.x * (tileId.x - 0)) * zMinMax.x / cbCullParams.proj_11;
	float nearMaxX = -(1 - 2.0f / actualScreenTiles.x * (tileId.x + 1)) * zMinMax.x / cbCullParams.proj_11;
	float nearMinY = (1 - 2.0f / actualScreenTiles.y * (tileId.y + 1)) * zMinMax.x / cbCullParams.proj_22;
	float nearMaxY = (1 - 2.0f / actualScreenTiles.y * (tileId.y + 0)) * zMinMax.x / cbCullParams.proj_22;

	float farMinX = -(1 - 2.0f / actualScreenTiles.x * (tileId.x - 0)) * zMinMax.y / cbCullParams.proj_11;
	float farMaxX = -(1 - 2.0f / actualScreenTiles.x * (tileId.x + 1)) * zMinMax.y / cbCullParams.proj_11;
	float farMinY = (1 - 2.0f / actualScreenTiles.y * (tileId.y + 1)) * zMinMax.y / cbCullParams.proj_22;
	float farMaxY = (1 - 2.0f / actualScreenTiles.y * (tileId.y + 0)) * zMinMax.y / cbCullParams.proj_22;

	float3 nbl = float3(nearMinX, nearMinY, zMinMax.x);
	float3 ntl = float3(nearMinX, nearMaxY, zMinMax.x);
	float3 nbr = float3(nearMaxX, nearMinY, zMinMax.x);
	float3 ntr = float3(nearMaxX, nearMaxY, zMinMax.x);

	float3 fbl = float3(farMinX, farMinY, zMinMax.y);
	float3 ftl = float3(farMinX, farMaxY, zMinMax.y);
	float3 fbr = float3(farMaxX, farMinY, zMinMax.y);
	float3 ftr = float3(farMaxX, farMaxY, zMinMax.y);

	frustum.planes[0] = planeFromPoints(ntl, ftl, fbl); // Left
	frustum.planes[1] = planeFromPoints(ftr, ntr, fbr); // Right - This is weird.  Using nbr for the third coord (instead of fbr) causes broken tiles.
	frustum.planes[2] = planeFromPoints(ntl, ntr, ftl); // Top
	frustum.planes[3] = planeFromPoints(nbr, nbl, fbl); // Bottom
	frustum.planes[4] = float4(0.f, 0.f, 1.f, -zMinMax.x); // Front
	frustum.planes[5] = float4(0.f, 0.f, -1.f, zMinMax.y); // Back
}

int overlapFrustum(in Frustum frustum, in float3 lightWorldPos, in float lightRadius)
{
	int inFrustum = 1;

	float4 lightViewPos = mul(float4(lightWorldPos, 1), cbCullParams.mtxView);

	for (int i = 0; i < 6; ++i)
	{
		float dist = dot(frustum.planes[i], lightViewPos);
		inFrustum = inFrustum && (dist + lightRadius) >= 0.f;
	}

	return inFrustum;
}

groupshared uint grp_tileLightCount;
groupshared uint grp_spotLightCount;
groupshared uint grp_pointLightCount;

[numthreads(THREAD_COUNT_X, THREAD_COUNT_Y, 1)]
void main( uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex)
{
	if (localIdx == 0)
	{
		grp_tileLightCount = 0;
		grp_spotLightCount = 0;
		grp_pointLightCount = 0;
	}

	uint tileIdx = (groupId.x + groupId.y * cbCullParams.tileCountX) * TILEDLIGHTING_BLOCK_SIZE;
	float2 zMinMax = g_depthMinMax.Load( uint3(groupId.xy, 0) );

	// create frustum
	Frustum frustum;
	constructFrustum(groupId.xy, zMinMax, frustum);

	uint i;
	for (i = localIdx; i < cbCullParams.spotLightCount; i += THREAD_COUNT_TOTAL)
	{
		SpotLight light = g_spotLights[i];
		if (overlapFrustum(frustum, light.position, light.radius))
		{
			// add light to light list
			uint lightIndex;
			InterlockedAdd(grp_tileLightCount, 1, lightIndex);

			if (lightIndex >= TILEDLIGHTING_MAX_LIGHTS_PER)
			{
				// Make sure we're not over the light limit before writing to the buffer.
				// The light count is still incremented, but is clamped to the max before writing.
				break;
			}

			InterlockedAdd(grp_spotLightCount, 1);
			g_tileLightIndices[tileIdx + lightIndex + LIGHTLIST_NUM_LIGHT_TYPES] = i;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	for (i = localIdx; i < cbCullParams.pointLightCount; i += THREAD_COUNT_TOTAL)
	{
		PointLight light = g_pointLights[i];
		if (overlapFrustum(frustum, light.position, light.radius))
		{
			// add light to light list
			uint lightIndex;
			InterlockedAdd(grp_tileLightCount, 1, lightIndex);

			if (lightIndex >= TILEDLIGHTING_MAX_LIGHTS_PER)
			{
				// Make sure we're not over the light limit before writing to the buffer.
				// The light count is still incremented, but is clamped to the max before writing.
				break;
			}

			InterlockedAdd(grp_pointLightCount, 1);
			g_tileLightIndices[tileIdx + lightIndex + LIGHTLIST_NUM_LIGHT_TYPES] = i;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if (localIdx == 0)
	{
		g_tileLightIndices[tileIdx + 0] = grp_spotLightCount;
		g_tileLightIndices[tileIdx + 1] = grp_pointLightCount;
	}
}