#include "light_types.h"

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

[numthreads(1, 1, 1)]
void main( uint3 groupId : SV_GroupID )
{
	uint tileIdx = groupId.x + groupId.y * cbCullParams.tileCountX;

	float2 zMinMax = g_depthMinMax.Load( uint3(groupId.xy, 0) );

	// create frustum
	Frustum frustum;
	constructFrustum(groupId.xy, zMinMax, frustum);

	uint tileLightCount = 0;
	for (uint i = 0; i < cbCullParams.lightCount; ++i)
	{
		if ( overlapFrustum(frustum, g_lights[i]))
		{
			// add light to tile's light list
			g_tileLightIndices[tileIdx * TILEDLIGHTING_MAX_LIGHTS_PER + tileLightCount + 1] = i;
			++tileLightCount;
			if (tileLightCount >= TILEDLIGHTING_MAX_LIGHTS_PER - 1)
				break;
		}
	}

	// todo: multi-threaded culling?

	g_tileLightIndices[tileIdx * TILEDLIGHTING_MAX_LIGHTS_PER] = tileLightCount;
}