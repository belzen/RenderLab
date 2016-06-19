#include "light_types.h"

#define THREAD_COUNT_X 8
#define THREAD_COUNT_Y 8
#define THREAD_COUNT_Z 1
#define THREAD_COUNT_TOTAL (THREAD_COUNT_X * THREAD_COUNT_Y * THREAD_COUNT_Z)

// Culling modes: 
#define CULL_AABB 1
#define CULL_FRUSTUM 2
#define CULL_FRUSTUM_SEPARATING_AXIS 3

#define CULLING_MODE CULL_FRUSTUM_SEPARATING_AXIS

cbuffer CullingParams : register(b0)
{
	ClusteredLightCullingParams cbCullParams;
};

StructuredBuffer<ShaderLight> g_lights : register(t0);
RWBuffer<uint> g_clusterLightIndices : register(u0);

struct Frustum
{
	float4 planes[6];
	float4 nearMinXMaxXMinYMaxY;
	float4 farMinXMaxXMinYMaxY;
	float3 center;
	float2 minMaxZ;
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

	frustum.nearMinXMaxXMinYMaxY = float4(nbl.x, nbr.x, nbl.y, ntl.y);
	frustum.farMinXMaxXMinYMaxY = float4(fbl.x, fbr.x, fbl.y, ftl.y);
	frustum.center = (bounds.min + bounds.max) * 0.5f;
	frustum.minMaxZ = float2(depthMin, depthMax);
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
	float4 lightPos = mul(float4(light.position, 1), cbCullParams.mtxView);

#if CULLING_MODE == CULL_FRUSTUM_SEPARATING_AXIS
	float3 clusterToLight = frustum.center - lightPos.xyz;
	float3 normal = normalize(clusterToLight);

	//http://www.jonmanatee.com/blog/2015/1/20/improved-spotlight-culling-for-clustered-forward-shading.html
	/*
	float min1 = -dot(normal, lightPos);
	float min2 = min1;
	min1 += min(normal.x * frustum.nearMinXMaxXMinYMaxY.x, normal.x * frustum.nearMinXMaxXMinYMaxY.y);
	min1 += min(normal.y * frustum.nearMinXMaxXMinYMaxY.z, normal.x * frustum.nearMinXMaxXMinYMaxY.w);
	min1 += normal.z * frustum.minMaxZ.x;

	min2 += min(normal.x * frustum.farMinXMaxXMinYMaxY.x, normal.x * frustum.farMinXMaxXMinYMaxY.y);
	min2 += min(normal.y * frustum.farMinXMaxXMinYMaxY.z, normal.y * frustum.farMinXMaxXMinYMaxY.w);
	min2 += normal.z * frustum.minMaxZ.y;

	float dist = min(min1, min2);*/

	float3 pt = float3(frustum.nearMinXMaxXMinYMaxY.x, frustum.nearMinXMaxXMinYMaxY.z, frustum.minMaxZ.x) - lightPos.xyz;
	float minDist = dot(normal, pt);

	pt = float3(frustum.nearMinXMaxXMinYMaxY.y, frustum.nearMinXMaxXMinYMaxY.z, frustum.minMaxZ.x) - lightPos.xyz;
	minDist = min(minDist, dot(normal, pt));

	pt = float3(frustum.nearMinXMaxXMinYMaxY.x, frustum.nearMinXMaxXMinYMaxY.w, frustum.minMaxZ.x) - lightPos.xyz;
	minDist = min(minDist, dot(normal, pt));

	pt = float3(frustum.nearMinXMaxXMinYMaxY.y, frustum.nearMinXMaxXMinYMaxY.w, frustum.minMaxZ.x) - lightPos.xyz;
	minDist = min(minDist, dot(normal, pt));

	pt = float3(frustum.farMinXMaxXMinYMaxY.x, frustum.farMinXMaxXMinYMaxY.z, frustum.minMaxZ.y) - lightPos.xyz;
	minDist = min(minDist, dot(normal, pt));

	pt = float3(frustum.farMinXMaxXMinYMaxY.y, frustum.farMinXMaxXMinYMaxY.z, frustum.minMaxZ.y) - lightPos.xyz;
	minDist = min(minDist, dot(normal, pt));

	pt = float3(frustum.farMinXMaxXMinYMaxY.x, frustum.farMinXMaxXMinYMaxY.w, frustum.minMaxZ.y) - lightPos.xyz;
	minDist = min(minDist, dot(normal, pt));

	pt = float3(frustum.farMinXMaxXMinYMaxY.y, frustum.farMinXMaxXMinYMaxY.w, frustum.minMaxZ.y) - lightPos.xyz;
	minDist = min(minDist, dot(normal, pt));

	return minDist < light.radius;

#else
	int inFrustum = 1;
	for (int i = 0; i < 6; ++i)
	{
		float dist = dot(frustum.planes[i], lightPos);
		inFrustum = inFrustum && (dist + light.radius) >= 0.f;
	}

	return inFrustum;
#endif
}

groupshared uint grp_clusterLightCount = 0;

[numthreads(THREAD_COUNT_X, THREAD_COUNT_Y, THREAD_COUNT_Z)]
void main(uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex)
{
	if (localIdx == 0)
	{
		grp_clusterLightCount = 0;
	}

	int clusterIdx = groupId.x + groupId.y * cbCullParams.clusterCountX + groupId.z * cbCullParams.clusterCountX * cbCullParams.clusterCountY;

	// create frustum
	AABB bounds;
	Frustum frustum;
	constructFrustum(groupId, frustum, bounds);

	for (uint i = localIdx; i < cbCullParams.lightCount; i += THREAD_COUNT_TOTAL)
	{
#if CULLING_MODE == CULL_AABB
		if (overlapAABB(bounds, g_lights[i]))
#else
		if (overlapFrustum(frustum, g_lights[i]))
#endif
		{
			// add light to light list
			uint lightIndex;
			InterlockedAdd(grp_clusterLightCount, 1, lightIndex);

			if (lightIndex >= CLUSTEREDLIGHTING_MAX_LIGHTS_PER - 1)
			{
				// Make sure we're not over the light limit before writing to the buffer.
				// The light count is still incremented, but is clamped to the max before writing.
				break;
			}

			g_clusterLightIndices[clusterIdx * CLUSTEREDLIGHTING_MAX_LIGHTS_PER + lightIndex + 1] = i;
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if (localIdx == 0)
	{
		g_clusterLightIndices[clusterIdx * CLUSTEREDLIGHTING_MAX_LIGHTS_PER] = min(grp_clusterLightCount, CLUSTEREDLIGHTING_MAX_LIGHTS_PER - 1);
	}
}