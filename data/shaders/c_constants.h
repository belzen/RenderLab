#ifndef SHADER_C_CONSTANTS_H
#define SHADER_C_CONSTANTS_H

#define BLUR_TEXELS_PER_THREAD 4
#define BLUR_THREAD_COUNT 64
#define BLUR_SIZE 9
#define BLUR_HALFSIZE 4

#define ADD_THREADS_X 8
#define ADD_THREADS_Y 8

#define BLEND_THREADS_X 32
#define BLEND_THREADS_Y 16

#define CLEAR_THREADS_X 8
#define CLEAR_THREADS_Y 8

#define COPY_THREADS_X 8
#define COPY_THREADS_Y 8

#define ATM_LUT_THREADS_X 8
#define ATM_LUT_THREADS_Y 8

#define VOLFOG_LUT_THREADS_X 8
#define VOLFOG_LUT_THREADS_Y 8

#define SHRINK_THREADS_X 8
#define SHRINK_THREADS_Y 8

struct ToneMapInputParams
{
	float white;
	float middleGrey;
	float minExposure;
	float maxExposure;

	float bloomThreshold;
	float frameTime;
	float2 unused;
};

struct Blend2dParams
{
	float2 size1;
	float weight;
	float unused;
};

struct AtmosphereParams
{
	// Distances are all in kilometers
	float planetRadius;
	float atmosphereHeight;
	float atmosphereRadius; // (Planet Radius + Atmosphere Height)
	float mieG;

	float3 mieScatteringCoeff;
	float mieAltitudeScale;

	float3 rayleighScatteringCoeff;
	float rayleighAltitudeScale;

	float3 ozoneExtinctionCoeff;
	float averageGroundReflectance;

	float3 sunDirection;
	float unused2;

	float3 sunColor;
	float unused3;
};

struct VolumetricFogParams
{
	uint3 lutSize;
	float farDepth;

	float3 scatteringCoeff;
	float phaseG;

	float3 absorptionCoeff;
	float unused;
};

#endif // SHADER_C_CONSTANTS_H