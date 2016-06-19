#include "light_types.h"

#define THREAD_COUNT_X 8
#define THREAD_COUNT_Y 8
#define THREAD_COUNT_Z 1
#define THREAD_COUNT_TOTAL (THREAD_COUNT_X * THREAD_COUNT_Y * THREAD_COUNT_Z)

// Culling modes: 
#define CULL_AABB 1
#define CULL_FRUSTUM 2
#define CULL_FRUSTUM_SEPARATING_AXIS 3

#define NEAR_CULLING_MODE CULL_AABB
#define FAR_CULLING_MODE CULL_FRUSTUM

cbuffer CullingParams : register(b0)
{
	ClusteredLightCullingParams cbCullParams;
};

StructuredBuffer<Light> g_lights : register(t0);
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

	// Get the non-rounded number of clusters on the screen.  
	// This is necessary for screens that are not divisible by the cluster size.
	// We're converting from NDC space to view space, so we need the actual visible clusters on the screen, not the rounded number.
	float2 actualScreenClusters = cbCullParams.screenSize / CLUSTEREDLIGHTING_TILE_SIZE;

	// Convert from NDC [-1, 1] space to view space.
	float nearMinX = -(1 - 2.0f / actualScreenClusters.x * (clusterId.x - 0) ) * depthMin / cbCullParams.proj_11;
	float nearMaxX = -(1 - 2.0f / actualScreenClusters.x * (clusterId.x + 1)) * depthMin / cbCullParams.proj_11;
	float nearMinY = (1 - 2.0f / actualScreenClusters.y * (clusterId.y + 1)) * depthMin / cbCullParams.proj_22;
	float nearMaxY = (1 - 2.0f / actualScreenClusters.y * (clusterId.y + 0)) * depthMin / cbCullParams.proj_22;

	float farMinX = -(1 - 2.0f / actualScreenClusters.x * (clusterId.x - 0)) * depthMax / cbCullParams.proj_11;
	float farMaxX = -(1 - 2.0f / actualScreenClusters.x * (clusterId.x + 1)) * depthMax / cbCullParams.proj_11;
	float farMinY = (1 - 2.0f / actualScreenClusters.y * (clusterId.y + 1)) * depthMax / cbCullParams.proj_22;
	float farMaxY = (1 - 2.0f / actualScreenClusters.y * (clusterId.y + 0)) * depthMax / cbCullParams.proj_22;

	float3 nbl = float3(nearMinX, nearMinY, depthMin);
	float3 ntl = float3(nearMinX, nearMaxY, depthMin);
	float3 nbr = float3(nearMaxX, nearMinY, depthMin);
	float3 ntr = float3(nearMaxX, nearMaxY, depthMin);

	float3 fbl = float3(farMinX, farMinY, depthMax);
	float3 ftl = float3(farMinX, farMaxY, depthMax);
	float3 fbr = float3(farMaxX, farMinY, depthMax);
	float3 ftr = float3(farMaxX, farMaxY, depthMax);

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

int overlapAABB(in AABB bounds, in Light light)
{
	float4 lightPos = mul(float4(light.position, 1), cbCullParams.mtxView);

	float distSqr = calcDistance(lightPos.x, bounds.min.x, bounds.max.x);
	distSqr += calcDistance(lightPos.y, bounds.min.y, bounds.max.y);
	distSqr += calcDistance(lightPos.z, bounds.min.z, bounds.max.z);

	return (distSqr < light.radius * light.radius);
}


int testFrustumSeparatingAxis(in Frustum frustum, in Light light)
{
	float4 lightPos = mul(float4(light.position, 1), cbCullParams.mtxView);

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
}

int overlapFrustum(in Frustum frustum, in Light light)
{
	float4 lightPos = mul(float4(light.position, 1), cbCullParams.mtxView);

	int inFrustum = 1;
	for (int i = 0; i < 6; ++i)
	{
		float dist = dot(frustum.planes[i], lightPos);
		inFrustum = inFrustum && (dist + light.radius) >= 0.f;
	}

	return inFrustum;
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

	// Split culling mode based on cluster depth.
	// Closer to the camera it's more effective to use AABB or frustum separating axis,
	// but further away when the frustums are larger, we get fewer false positives with a basic frustum test.
	if (groupId.z <= 8)
	{
		for (uint i = localIdx; i < cbCullParams.lightCount; i += THREAD_COUNT_TOTAL)
		{
#if NEAR_CULLING_MODE == CULL_AABB
			if (overlapAABB(bounds, g_lights[i]))
#elif NEAR_CULLING_MODE == CULL_FRUSTUM_SEPARATING_AXIS
			if (testFrustumSeparatingAxis(frustum, g_lights[i]))
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
	}
	else
	{
		for (uint i = localIdx; i < cbCullParams.lightCount; i += THREAD_COUNT_TOTAL)
		{
#if FAR_CULLING_MODE == CULL_AABB
			if (overlapAABB(bounds, g_lights[i]))
#elif FAR_CULLING_MODE == CULL_FRUSTUM_SEPARATING_AXIS
			if (testFrustumSeparatingAxis(frustum, g_lights[i]))
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
	}

	GroupMemoryBarrierWithGroupSync();

	if (localIdx == 0)
	{
		g_clusterLightIndices[clusterIdx * CLUSTEREDLIGHTING_MAX_LIGHTS_PER] = min(grp_clusterLightCount, CLUSTEREDLIGHTING_MAX_LIGHTS_PER - 1);
	}
}