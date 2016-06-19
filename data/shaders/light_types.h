#ifndef LIGHT_TYPES_H
#define LIGHT_TYPES_H

struct ShaderLight
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

struct ShaderShadowData
{
	float4x4 mtxViewProj;
	float partitionEndZ;
};

struct ClusteredLightCullingParams
{
	float4x4 mtxView;

	float fovY;
	float aspectRatio;
	float2 screenSize;

	uint lightCount;
	uint clusterCountX;
	uint clusterCountY;
	uint clusterCountZ;
};

struct TiledLightCullingParams
{
	float4x4 mtxView;

	float fovY;
	float aspectRatio;
	float cameraNearDist;
	float cameraFarDist;

	float2 screenSize;
	float2 unused;

	uint lightCount;
	uint tileCountX;
	uint tileCountY;
	float unused2;
};

#define MAX_SHADOW_MAPS 10
#define MAX_SHADOW_CUBEMAPS 2

#define TILEDLIGHTING_TILE_SIZE 16
#define TILEDLIGHTING_MAX_LIGHTS_PER 128

#define CLUSTEREDLIGHTING_TILE_SIZE 64
#define CLUSTEREDLIGHTING_DEPTH_SLICES 16
#define CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH 5.f
#define CLUSTEREDLIGHTING_MAX_DEPTH 1500.f
#define CLUSTEREDLIGHTING_MAX_LIGHTS_PER 32

#endif // LIGHT_TYPES_H