#include "light_types.h"

#define THREAD_COUNT_X 8
#define THREAD_COUNT_Y 8
#define THREAD_COUNT_TOTAL (THREAD_COUNT_X * THREAD_COUNT_Y)

cbuffer CullingParams : register(b0)
{
	TiledLightCullingParams cbCullParams;
};

StructuredBuffer<ShaderLight> g_lights : register(t0);
Texture2D<float2> g_depthMinMax : register(t1);

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
	// Half widths
	float hNear = tan(cbCullParams.fovY * 0.5f) * cbCullParams.cameraNearDist;
	float wNear = hNear * cbCullParams.aspectRatio;
	
	float hNearTile = hNear * (float(TILEDLIGHTING_TILE_SIZE) / cbCullParams.screenSize.y);
	float wNearTile = wNear * (float(TILEDLIGHTING_TILE_SIZE) / cbCullParams.screenSize.x);

	float hFar = tan(cbCullParams.fovY * 0.5f) * cbCullParams.cameraFarDist;
	float wFar = hFar * cbCullParams.aspectRatio;
	float hFarTile = hFar * (float(TILEDLIGHTING_TILE_SIZE) / cbCullParams.screenSize.y);
	float wFarTile = wFar * (float(TILEDLIGHTING_TILE_SIZE) / cbCullParams.screenSize.x);

	float3 up = float3(0.f, 1.f, 0.f);
	float3 right = float3(1.f, 0.f, 0.f);

	float3 nc = float3(0.f, 0.f, cbCullParams.cameraNearDist);
	float3 ntl = nc - (wNear * right) 
		+ (right * tileId.x * wNearTile * 2.f)
		+ (hNear * up)
		- (up * tileId.y * hNearTile * 2.f);
	float3 ntr = ntl + wNearTile * right * 2.f;
	float3 nbl = ntl - hNearTile * up * 2.f;
	float3 nbr = nbl + wNearTile * right * 2.f;

	float3 fc = float3(0.f, 0.f, cbCullParams.cameraFarDist);
	float3 ftl = fc - (wFar * right)
		+ (right * tileId.x * wFarTile * 2.f)
		+ (hFar * up)
		- (up * tileId.y * hFarTile * 2.f);
	float3 ftr = ftl + wFarTile * right * 2.f;
	float3 fbl = ftl - hFarTile * up * 2.f;
	float3 fbr = fbl + wFarTile * right * 2.f;

	frustum.planes[0] = planeFromPoints(ntl, ftl, fbl); // Left
	frustum.planes[1] = planeFromPoints(ftr, ntr, fbr); // Right - This is weird.  Using nbr for the third coord (instead of fbr) causes broken tiles.
	frustum.planes[2] = planeFromPoints(ntl, ntr, ftl); // Top
	frustum.planes[3] = planeFromPoints(nbr, nbl, fbl); // Bottom
	frustum.planes[4] = float4(0.f, 0.f, 1.f, -zMinMax.x); // Front
	frustum.planes[5] = float4(0.f, 0.f, -1.f, zMinMax.y); // Back
}

int overlapFrustum(in Frustum frustum, in ShaderLight light)
{
	int inFrustum = 1;

	float4 lightPos = mul(float4(light.position, 1), cbCullParams.mtxView);

	for (int i = 0; i < 6; ++i)
	{
		float dist = dot(frustum.planes[i], lightPos);
		inFrustum = inFrustum && (dist + light.radius) >= 0.f;
	}

	return inFrustum;
}

groupshared uint grp_tileLightCount = 0;

[numthreads(THREAD_COUNT_X, THREAD_COUNT_Y, 1)]
void main( uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex)
{
	if (localIdx == 0)
	{
		grp_tileLightCount = 0;
	}

	uint tileIdx = groupId.x + groupId.y * cbCullParams.tileCountX;
	float2 zMinMax = g_depthMinMax.Load( uint3(groupId.xy, 0) );

	// create frustum
	Frustum frustum;
	constructFrustum(groupId.xy, zMinMax, frustum);

	for (uint i = localIdx; i < cbCullParams.lightCount; i += THREAD_COUNT_TOTAL)
	{
		if (overlapFrustum(frustum, g_lights[i]))
		{
			// add light to light list
			uint lightIndex;
			InterlockedAdd(grp_tileLightCount, 1, lightIndex);

			if (lightIndex >= CLUSTEREDLIGHTING_MAX_LIGHTS_PER - 1)
			{
				// Make sure we're not over the light limit before writing to the buffer.
				// The light count is still incremented, but is clamped to the max before writing.
				break;
			}

			g_tileLightIndices[tileIdx * TILEDLIGHTING_MAX_LIGHTS_PER + lightIndex + 1] = i;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if (localIdx == 0)
	{
		g_tileLightIndices[tileIdx * TILEDLIGHTING_MAX_LIGHTS_PER] = min(grp_tileLightCount, TILEDLIGHTING_MAX_LIGHTS_PER - 1);
	}
}