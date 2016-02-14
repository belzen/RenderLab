
#define LIGHT_DATA_ONLY 1
#include "light_inc.hlsli"

cbuffer CullingParams : register(b0)
{
	float3 cameraPos;
	float fovY;

	float3 cameraDir;
	float aspectRatio;

	float cameraNearDist;
	float cameraFarDist;

	uint lightCount;
	uint tileCountX;
	uint tileCountY;
};

StructuredBuffer<Light> g_lights : register(t0);
Texture2D<float2> g_depthMinMax : register(t1);

RWStructuredBuffer<uint> g_tileLightIndices : register(u0);

struct Frustum
{
	float4 planes[4]; // Left, right, top, bottom.
};

float4 planeFromPoints(float3 pt1, float3 pt2, float3 pt3)
{
	float3 v21 = pt1 - pt2;
	float3 v31 = pt1 - pt3;

	float3 n = normalize( cross(v21, v31) );

	float d = dot(n, pt1);
	return float4(n.xyz, -d);
}

// TODO: Faster to convert light bounds to screen space and aabb cull?
void constructFrustum(uint tileId, float2 zMinMax, out Frustum frustum)
{
	int tileX = tileId % tileCountX;
	int tileY = (tileId / tileCountY);

	float hNear = tan(fovY * 0.5f) * cameraNearDist;
	float wNear = hNear * aspectRatio;
	hNear /= tileCountY;
	wNear /= tileCountX;

	float hFar = tan(fovY * 0.5f) * cameraFarDist;
	float wFar = hFar * aspectRatio;
	hFar /= tileCountY;
	wFar /= tileCountX;

	float3 up = float3(0.f, 1.f, 0.f);
	float3 right = normalize( cross(up, cameraDir) );
	up = normalize( cross(cameraDir, right) );


	float3 nc = cameraPos + cameraDir * cameraNearDist;
	float3 ntl = nc - (wNear * right * tileCountX) 
		+ (right * tileX * wNear * 2.f) 
		+ (hNear * up * tileCountY) +
		- (up * tileY * hNear * 2.f);
	float3 ntr = ntl + wNear * right * 2.f;
	float3 nbl = ntl - hNear * up * 2.f;
	float3 nbr = nbl + wNear * right * 2.f;

	float3 fc = cameraPos + cameraDir * cameraFarDist;
	float3 ftl = fc - (wFar * right * tileCountX)
		+ (right * tileX * wFar * 2.f)
		+ (hFar * up * tileCountY) +
		-(up * tileY * hFar * 2.f);
	float3 ftr = ftl + wFar * right * 2.f;
	float3 fbl = ftl - hFar * up * 2.f;
	float3 fbr = fbl + wFar * right * 2.f;

	frustum.planes[0] = planeFromPoints(ntl, ftl, fbl); // Left
	frustum.planes[1] = planeFromPoints(ftr, ntr, nbr); // Right
	frustum.planes[2] = planeFromPoints(ntl, ntr, ftl); // Top
	frustum.planes[3] = planeFromPoints(nbr, nbl, fbl); // Bottom
}

int overlapFrustum(in Frustum frustum, in Light light)
{
	int above = 0;

	for (int i = 0; i < 4; ++i)
	{
		float dist = dot(frustum.planes[i].xyz, light.position) + frustum.planes[i].w;
		above = above || (dist + light.radius) >= 0.f;
	}

	return above;
}

int overlapDepth(in float2 zMinMax, in Light light)
{
	float3 viewPos = light.position - cameraPos;
	float d = dot(cameraDir, viewPos);
	return (d + light.radius >= zMinMax.x) && (d - light.radius <= zMinMax.y);
}


[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID, uint3 groupId : SV_GroupID )
{
	int tileId = groupId.x + groupId.y * tileCountX;

	float2 zMinMax = g_depthMinMax.Load( uint3(groupId.xy, 0) );

	// create frustum
	Frustum frustum;
	constructFrustum(tileId, zMinMax, frustum);

	uint tileLightCount = 0;
	for (uint i = 0; i < lightCount; ++i)
	{
		if (overlapFrustum( frustum, g_lights[i] ) && overlapDepth(zMinMax, g_lights[i]) )
		{
			// add light to tile's light list
			g_tileLightIndices[tileId * MAX_LIGHTS_PER_TILE + tileLightCount + 1] = i;
			++tileLightCount;
			if (tileLightCount >= MAX_LIGHTS_PER_TILE - 1)
				break;
		}
	}

	// todo: multi-threaded culling?

	g_tileLightIndices[tileId * MAX_LIGHTS_PER_TILE] = tileLightCount;
}