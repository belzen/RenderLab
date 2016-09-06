#ifndef SHADER_V_CONSTANTS_H
#define SHADER_V_CONSTANTS_H

struct VsPerAction
{
	float4x4 mtxViewProj;
	float3 cameraPosition;
};

struct VsPerObject
{
	float4x4 mtxWorld;
};

#endif // SHADER_V_CONSTANTS_H
