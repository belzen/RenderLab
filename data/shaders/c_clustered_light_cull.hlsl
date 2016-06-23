#include "light_types.h"

#define THREAD_COUNT_X 8
#define THREAD_COUNT_Y 8
#define THREAD_COUNT_Z 1
#define THREAD_COUNT_TOTAL (THREAD_COUNT_X * THREAD_COUNT_Y * THREAD_COUNT_Z)

cbuffer CullingParams : register(b0)
{
	ClusteredLightCullingParams cbCullParams;
};

StructuredBuffer<SpotLight> g_spotLights : register(t0);
StructuredBuffer<PointLight> g_pointLights : register(t1);

RWBuffer<uint> g_clusterLightIndices : register(u0);

struct Frustum
{
	float4 planes[6];
	float4 nearMinXMaxXMinYMaxY;
	float4 farMinXMaxXMinYMaxY;
	float3 center;
	float2 minMaxZ;
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

void constructFrustum(uint3 clusterId, out Frustum frustum)
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

	frustum.min = min(nbl, fbl);
	frustum.max = max(ntr, ftr);

	frustum.nearMinXMaxXMinYMaxY = float4(nbl.x, nbr.x, nbl.y, ntl.y);
	frustum.farMinXMaxXMinYMaxY = float4(fbl.x, fbr.x, fbl.y, ftl.y);
	frustum.center = (frustum.min + frustum.max) * 0.5f;
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

int overlapAABB(in Frustum frustum, in float3 lightViewPos, in float lightRadius)
{
	float distSqr = calcDistance(lightViewPos.x, frustum.min.x, frustum.max.x);
	distSqr += calcDistance(lightViewPos.y, frustum.min.y, frustum.max.y);
	distSqr += calcDistance(lightViewPos.z, frustum.min.z, frustum.max.z);

	return (distSqr < lightRadius * lightRadius);
}

int testFrustumAxis(in Frustum frustum, in float3 pos, in float3 axisNormal, in float radius)
{
	float min1 = -dot(axisNormal, pos.xyz);
	float min2 = min1;

	min1 += min(axisNormal.x * frustum.nearMinXMaxXMinYMaxY.x, axisNormal.x * frustum.nearMinXMaxXMinYMaxY.y);
	min1 += min(axisNormal.y * frustum.nearMinXMaxXMinYMaxY.z, axisNormal.y * frustum.nearMinXMaxXMinYMaxY.w);
	min1 += axisNormal.z * frustum.minMaxZ.x;

	min2 += min(axisNormal.x * frustum.farMinXMaxXMinYMaxY.x, axisNormal.x * frustum.farMinXMaxXMinYMaxY.y);
	min2 += min(axisNormal.y * frustum.farMinXMaxXMinYMaxY.z, axisNormal.y * frustum.farMinXMaxXMinYMaxY.w);
	min2 += axisNormal.z * frustum.minMaxZ.y;

	float minDistance = min(min1, min2);
	return minDistance < radius;
}

int overlapFrustum(in Frustum frustum, in float4 lightViewPos, in float lightRadius)
{
	int inFrustum = 1;
	for (int i = 0; i < 6; ++i)
	{
		float dist = dot(frustum.planes[i], lightViewPos);
		inFrustum = inFrustum && (dist + lightRadius) >= 0.f;
	}

	return inFrustum;
}

bool cullPointLight(in Frustum frustum, in PointLight light, int nearCull)
{
	float4 lightViewPos = mul(float4(light.position, 1), cbCullParams.mtxView);

	// This should be optimized out since nearCull is always provided as a constant.
	if (nearCull)
	{
		float3 dirToCluster = normalize(frustum.center - lightViewPos.xyz);
		return testFrustumAxis(frustum, lightViewPos.xyz, dirToCluster, light.radius);
	}
	else
	{
		return overlapFrustum(frustum, lightViewPos, light.radius);
	}
}

bool cullSpotLight(in Frustum frustum, in SpotLight light)
{
	//http://www.jonmanatee.com/blog/2015/1/20/improved-spotlight-culling-for-clustered-forward-shading.html
	float4 lightViewPos = mul(float4(light.position, 1), cbCullParams.mtxView);
	float3 dirToCluster = normalize(frustum.center - lightViewPos.xyz);

	// Coarse check vs spot light sphere.
	if (!testFrustumAxis(frustum, lightViewPos.xyz, dirToCluster, light.radius))
		return false;

	float3 lightViewDir = mul(float4(light.direction, 0), cbCullParams.mtxView).xyz;
	float3 planeTangent = normalize(cross(lightViewDir, dirToCluster));
	float3 capDir = cross(planeTangent, lightViewDir);

	// Transform the light's range/radius into the radius of the cone at the cap
	float coneBaseRadius = light.radius * tan(light.outerConeAngle);

	// Find closest point on the cone's cap to the frustum.
	float3 closestEndPoint = lightViewPos.xyz + (lightViewDir * light.radius) + (capDir * coneBaseRadius);

	float3 planeBitangent = normalize(closestEndPoint - lightViewPos.xyz);
	float3 planeNormal = cross(planeTangent, planeBitangent);

	return testFrustumAxis(frustum, lightViewPos.xyz, planeNormal, 0.f);
}

groupshared uint grp_clusterLightCount;
groupshared uint grp_spotLightCount;
groupshared uint grp_pointLightCount;

[numthreads(THREAD_COUNT_X, THREAD_COUNT_Y, THREAD_COUNT_Z)]
void main(uint3 groupId : SV_GroupID, uint localIdx : SV_GroupIndex)
{
	if (localIdx == 0)
	{
		grp_clusterLightCount = 0;
		grp_spotLightCount = 0;
		grp_pointLightCount = 0;
	}

	uint i;
	int clusterIdx = (groupId.x + groupId.y * cbCullParams.clusterCountX + groupId.z * cbCullParams.clusterCountX * cbCullParams.clusterCountY);
	clusterIdx *= CLUSTEREDLIGHTING_BLOCK_SIZE;

	// create frustum
	Frustum frustum;
	constructFrustum(groupId, frustum);

	// Split culling mode based on cluster depth.
	// Closer to the camera it's more effective to use AABB or frustum separating axis,
	// but further away when the frustums are larger, we get fewer false positives with a basic frustum test.
	if (groupId.z <= 8)
	{
		for (i = localIdx; i < cbCullParams.spotLightCount; i += THREAD_COUNT_TOTAL)
		{
			SpotLight light = g_spotLights[i];
			if (cullSpotLight(frustum, light))
			{
				// add light to light list
				uint lightIndex;
				InterlockedAdd(grp_clusterLightCount, 1, lightIndex);

				if (lightIndex >= CLUSTEREDLIGHTING_MAX_LIGHTS_PER)
				{
					// Make sure we're not over the light limit before writing to the buffer.
					// The light count is still incremented, but is clamped to the max before writing.
					break;
				}

				InterlockedAdd(grp_spotLightCount, 1);
				g_clusterLightIndices[clusterIdx + lightIndex + LIGHTLIST_NUM_LIGHT_TYPES] = i;
			}
		}

		GroupMemoryBarrierWithGroupSync();

		for (i = localIdx; i < cbCullParams.pointLightCount; i += THREAD_COUNT_TOTAL)
		{
			PointLight light = g_pointLights[i];
			if (cullPointLight(frustum, light, 1))
			{
				// add light to light list
				uint lightIndex;
				InterlockedAdd(grp_clusterLightCount, 1, lightIndex);

				if (lightIndex >= CLUSTEREDLIGHTING_MAX_LIGHTS_PER)
				{
					// Make sure we're not over the light limit before writing to the buffer.
					// The light count is still incremented, but is clamped to the max before writing.
					break;
				}

				InterlockedAdd(grp_pointLightCount, 1);
				g_clusterLightIndices[clusterIdx + lightIndex + LIGHTLIST_NUM_LIGHT_TYPES] = i;
			}
		}
	}
	else
	{
		for (i = localIdx; i < cbCullParams.spotLightCount; i += THREAD_COUNT_TOTAL)
		{
			SpotLight light = g_spotLights[i];
			if (cullSpotLight(frustum, light))
			{
				// add light to light list
				uint lightIndex;
				InterlockedAdd(grp_clusterLightCount, 1, lightIndex);

				if (lightIndex >= CLUSTEREDLIGHTING_MAX_LIGHTS_PER)
				{
					// Make sure we're not over the light limit before writing to the buffer.
					// The light count is still incremented, but is clamped to the max before writing.
					break;
				}

				InterlockedAdd(grp_spotLightCount, 1);
				g_clusterLightIndices[clusterIdx + lightIndex + LIGHTLIST_NUM_LIGHT_TYPES] = i;
			}
		}

		GroupMemoryBarrierWithGroupSync();

		for (i = localIdx; i < cbCullParams.pointLightCount; i += THREAD_COUNT_TOTAL)
		{
			PointLight light = g_pointLights[i];
			if (cullPointLight(frustum, light, 0))
			{
				// add light to light list
				uint lightIndex;
				InterlockedAdd(grp_clusterLightCount, 1, lightIndex);

				if (lightIndex >= CLUSTEREDLIGHTING_MAX_LIGHTS_PER)
				{
					// Make sure we're not over the light limit before writing to the buffer.
					// The light count is still incremented, but is clamped to the max before writing.
					break;
				}

				InterlockedAdd(grp_pointLightCount, 1);
				g_clusterLightIndices[clusterIdx + lightIndex + LIGHTLIST_NUM_LIGHT_TYPES] = i;
			}
		}
	}

	GroupMemoryBarrierWithGroupSync();

	if (localIdx == 0)
	{
		g_clusterLightIndices[clusterIdx + 0] = grp_spotLightCount;
		g_clusterLightIndices[clusterIdx + 1] = grp_pointLightCount;
	}
}
