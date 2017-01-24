#ifndef SHADER_P_CONSTANTS_H
#define SHADER_P_CONSTANTS_H

struct PsPerAction
{
	float4x4 mtxInvProj;

	float3 cameraPos;
	float cameraNearDist;

	float3 cameraDir;
	float cameraFarDist;

	uint2 viewSize;
	float cameraFovY;
	float aspectRatio;
};

struct ToneMapOutputParams
{
	float linearExposure;
	float white;
	float adaptedLum;
	float unused;
};

struct ToneMapHistogramParams
{
	float logLuminanceMin;
	float logLuminanceMax;
	uint tileCount;
};

struct MaterialParams
{
	float roughness;
	float metalness;
	float2 unused;
};

#endif // SHADER_P_CONSTANTS_H
