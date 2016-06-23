#ifndef LIGHT_TYPES_H
#define LIGHT_TYPES_H

struct DirectionalLight
{
	float3 direction;
	float unused;

	float3 color;
	uint shadowMapIndex;
};

struct DirectionalLightList
{
	DirectionalLight lights[8];
	uint numLights;
	float3 unused;
};

struct SpotLight
{
	float3 position;
	float innerConeAngleCos;

	float3 direction;
	float outerConeAngleCos;

	float3 color;
	uint shadowMapIndex;

	float radius;
	float outerConeAngle;
	float2 unused;
};

struct PointLight
{
	float3 position;
	float radius;

	float3 color;
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

	float proj_11;
	float proj_22;
	uint spotLightCount;
	uint pointLightCount;

	float fovY;
	float aspectRatio;
	float2 screenSize;

	uint clusterCountX;
	uint clusterCountY;
	uint clusterCountZ;
	uint unused;
};

struct TiledLightCullingParams
{
	float4x4 mtxView;

	float fovY;
	float aspectRatio;
	float cameraNearDist;
	float cameraFarDist;

	float2 screenSize;
	float proj_11;
	float proj_22;

	uint tileCountX;
	uint tileCountY;
	uint spotLightCount;
	uint pointLightCount;
};

#define MAX_SHADOW_MAPS 10
#define MAX_SHADOW_CUBEMAPS 2

#define LIGHTLIST_NUM_LIGHT_TYPES 2

#define TILEDLIGHTING_TILE_SIZE 16
#define TILEDLIGHTING_MAX_LIGHTS_PER 128
#define TILEDLIGHTING_BLOCK_SIZE (LIGHTLIST_NUM_LIGHT_TYPES + TILEDLIGHTING_MAX_LIGHTS_PER)

#define CLUSTEREDLIGHTING_TILE_SIZE 64
#define CLUSTEREDLIGHTING_DEPTH_SLICES 16
#define CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH 5.f
#define CLUSTEREDLIGHTING_MAX_DEPTH 1500.f
#define CLUSTEREDLIGHTING_MAX_LIGHTS_PER 64
#define CLUSTEREDLIGHTING_BLOCK_SIZE (LIGHTLIST_NUM_LIGHT_TYPES + CLUSTEREDLIGHTING_MAX_LIGHTS_PER)


#endif // LIGHT_TYPES_H