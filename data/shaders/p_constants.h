#ifndef SHADER_P_CONSTANTS_H
#define SHADER_P_CONSTANTS_H

struct PsPerAction
{
	float4x4 mtxView;
	float4x4 mtxProj;
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

	float4 color; // Optional color tint and alpha.
};

#define SSAO_KERNEL_SIZE 4
struct SsaoParams
{
	float4 sampleKernel[SSAO_KERNEL_SIZE * SSAO_KERNEL_SIZE];

	float2 noiseUvScale;
	float sampleRadius;
	float blurSize;

	float2 texelSize; // Texel size of the SSAO buffer.
	float2 unused2;
};

#endif // SHADER_P_CONSTANTS_H
