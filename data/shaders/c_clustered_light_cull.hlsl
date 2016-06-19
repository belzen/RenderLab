#include "light_types.h"

#define THREAD_COUNT_X 1
#define THREAD_COUNT_Y 1
#define THREAD_COUNT_ALL (THREAD_COUNT_X * THREAD_COUTN_Y)

#define AABB_CULL 1

cbuffer CullingParams : register(b0)
{
	ClusteredLightCullingParams cbCullParams;
};

StructuredBuffer<ShaderLight> g_lights : register(t0);
RWBuffer<uint> g_clusterLightIndices : register(u0);

struct Frustum
{
	float4 planes[6];
};

struct AABB
{
	float3 min;
	float3 max;
};

float4 planeFromPoints(float3 pt1, float3 pt2, float3 pt3)
{
	float3 v21 = pt1 - pt2;
	float3 v31 = pt1 - pt3;

	float3 n = normalize(cross(v21, v31));

	float d = dot(n, pt1);
	return float4(n.xyz, -d);
}

void constructFrustum(uint3 clusterId, out Frustum frustum, out AABB bounds)
{
	float depthMin = 0.1f;
	float depthMax = CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH;
	
	if (clusterId.z > 0)
	{
		depthMin = CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH * pow(CLUSTEREDLIGHTING_MAX_DEPTH / CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH, (clusterId.z - 1) / (CLUSTEREDLIGHTING_DEPTH_SLICES - 1.f));
		depthMax = CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH * pow(CLUSTEREDLIGHTING_MAX_DEPTH / CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH, (clusterId.z + 0) / (CLUSTEREDLIGHTING_DEPTH_SLICES - 1.f));
	}

	// Half widths
	float hNear = tan(cbCullParams.fovY * 0.5f) * depthMin;
	float wNear = hNear * cbCullParams.aspectRatio;

	float hNearTile = hNear * (float(CLUSTEREDLIGHTING_TILE_SIZE) / cbCullParams.screenSize.y);
	float wNearTile = wNear * (float(CLUSTEREDLIGHTING_TILE_SIZE) / cbCullParams.screenSize.x);

	float hFar = tan(cbCullParams.fovY * 0.5f) * depthMax;
	float wFar = hFar * cbCullParams.aspectRatio;
	float hFarTile = hFar * (float(CLUSTEREDLIGHTING_TILE_SIZE) / cbCullParams.screenSize.y);
	float wFarTile = wFar * (float(CLUSTEREDLIGHTING_TILE_SIZE) / cbCullParams.screenSize.x);

	float3 up = float3(0.f, 1.f, 0.f);
	float3 right = float3(1.f, 0.f, 0.f);

	float3 nc = float3(0.f, 0.f, depthMin);
	float3 ntl = nc - (wNear * right)
		+ (right * clusterId.x * wNearTile * 2.f)
		+ (hNear * up)
		- (up * clusterId.y * hNearTile * 2.f);
	float3 ntr = ntl + wNearTile * right * 2.f;
	float3 nbl = ntl - hNearTile * up * 2.f;
	float3 nbr = nbl + wNearTile * right * 2.f;

	float3 fc = float3(0.f, 0.f, depthMax);
	float3 ftl = fc - (wFar * right)
		+ (right * clusterId.x * wFarTile * 2.f)
		+ (hFar * up)
		- (up * clusterId.y * hFarTile * 2.f);
	float3 ftr = ftl + wFarTile * right * 2.f;
	float3 fbl = ftl - hFarTile * up * 2.f;
	float3 fbr = fbl + wFarTile * right * 2.f;

	frustum.planes[0] = planeFromPoints(ntl, ftl, fbl); // Left
	frustum.planes[1] = planeFromPoints(ftr, ntr, nbr); // Right
	frustum.planes[2] = planeFromPoints(ntl, ntr, ftl); // Top
	frustum.planes[3] = planeFromPoints(nbr, nbl, fbl); // Bottom
	frustum.planes[4] = float4(0.f, 0.f, 1.f, -depthMin); // Front
	frustum.planes[5] = float4(0.f, 0.f, -1.f, depthMax); // Back

	bounds.min = min(nbl, fbl);
	bounds.max = max(ntr, ftr);
}

float calcDistance(float val, float minVal, float maxVal)
{
	if (val < minVal)
	{
		float d = minVal - val;
		return d * d;
	}
	else if (val > maxVal)
	{
		float d = maxVal - val;
		return d * d;
	}
	return 0;
}

int overlapAABB(in AABB bounds, in ShaderLight light)
{
	float4 lightPos = mul(float4(light.position, 1), cbCullParams.mtxView);

	float distSqr = calcDistance(lightPos.x, bounds.min.x, bounds.max.x);
	distSqr += calcDistance(lightPos.y, bounds.min.y, bounds.max.y);
	distSqr += calcDistance(lightPos.z, bounds.min.z, bounds.max.z);

	return (distSqr < light.radius * light.radius);
}


int overlapFrustum(in Frustum frustum, in ShaderLight light)
{
	int inFrustum = 1;

	float4 lightPos = mul(float4(light.position,1), cbCullParams.mtxView);

	for (int i = 0; i < 6; ++i)
	{
		float dist = dot(frustum.planes[i], lightPos);
		inFrustum = inFrustum && (dist + light.radius) >= 0.f;
	}

	return inFrustum;
}

[numthreads(THREAD_COUNT_X, THREAD_COUNT_Y, 1)]
void main(uint3 groupId : SV_GroupID)
{
	int clusterIdx = groupId.x + groupId.y * cbCullParams.clusterCountX + groupId.z * cbCullParams.clusterCountX * cbCullParams.clusterCountY;

	// create frustum
	AABB bounds;
	Frustum frustum;
	constructFrustum(groupId, frustum, bounds);

	uint clusterLightCount = 0;
	for (uint i = 0; i < cbCullParams.lightCount; ++i)
	{
#if AABB_CULL
		if (overlapAABB(bounds, g_lights[i]))
#else
		if (overlapFrustum(frustum, g_lights[i]))
#endif
		{
			// add light to light list
			g_clusterLightIndices[clusterIdx * CLUSTEREDLIGHTING_MAX_LIGHTS_PER + clusterLightCount + 1] = i;
			++clusterLightCount;
			if (clusterLightCount >= CLUSTEREDLIGHTING_MAX_LIGHTS_PER - 1)
				break;
		}
	}

	g_clusterLightIndices[clusterIdx * CLUSTEREDLIGHTING_MAX_LIGHTS_PER] = clusterLightCount;
}