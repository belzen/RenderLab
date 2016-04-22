#ifndef SHADER_P_CONSTANTS_H
#define SHADER_P_CONSTANTS_H

struct PsPerAction
{
	float4x4 mtxInvProj;
	float3 viewPos;
	uint viewWidth;
};

struct ToneMapOutputParams
{
	float linearExposure;
	float white;
	float lumAvg;
	float unused;
};

#endif // SHADER_P_CONSTANTS_H
