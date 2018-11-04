#ifndef LIGHT_TYPES_H
#define LIGHT_TYPES_H

#define MAX_ENVIRONMENT_MAPS 8
#define MAX_SHADOW_MAPS 10
#define MAX_SHADOW_CUBEMAPS 2

struct DirectionalLight
{
	float3 direction;
	float pssmLambda;

	float3 color;
	uint shadowMapIndex;
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

struct EnvironmentLight
{
	float3 position;
	uint environmentMapIndex;
};

struct ShaderShadowData
{
	float4x4 mtxViewProj;
	float partitionEndZ;
	float3 unused;
};

struct GlobalLightData
{
	EnvironmentLight globalEnvironmentLight;
	DirectionalLight directionalLights[4];
	ShaderShadowData shadowData[MAX_SHADOW_MAPS];

	uint numDirectionalLights;
	uint clusterTileSize;
	float volumetricFogFarDepth;
	float unused;
};

struct ClusteredLightCullingParams
{
	float4x4 mtxView;
	float4x4 mtxProj;

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
	uint clusterTileSize;
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

#define LIGHTLIST_NUM_LIGHT_TYPES 2

#define TILEDLIGHTING_TILE_SIZE 16
#define TILEDLIGHTING_MAX_LIGHTS_PER 128
#define TILEDLIGHTING_BLOCK_SIZE (LIGHTLIST_NUM_LIGHT_TYPES + TILEDLIGHTING_MAX_LIGHTS_PER)

#define CLUSTEREDLIGHTING_DEPTH_SLICES 24
#define CLUSTEREDLIGHTING_SPECIAL_NEAR_DEPTH 5.f
#define CLUSTEREDLIGHTING_MAX_DEPTH 1500.f
#define CLUSTEREDLIGHTING_MAX_LIGHTS_PER 64
#define CLUSTEREDLIGHTING_BLOCK_SIZE (LIGHTLIST_NUM_LIGHT_TYPES + CLUSTEREDLIGHTING_MAX_LIGHTS_PER)

static const float3 ambient_color = float3(1.f, 1.f, 1.f);
static const float ambient_intensity = 0.05f;

#endif // LIGHT_TYPES_H