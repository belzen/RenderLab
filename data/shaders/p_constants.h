#ifndef SHADER_P_CONSTANTS_H
#define SHADER_P_CONSTANTS_H

struct PsPerAction
{
	float4x4 mtxInvProj;
	float3 cameraPos;
	float unused;
	float3 cameraDir;
	float unused2;
	uint2 viewSize;
	float2 unused3;
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

#endif // SHADER_P_CONSTANTS_H
