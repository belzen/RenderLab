#ifndef SHADER_V_CONSTANTS_H
#define SHADER_V_CONSTANTS_H

struct VsPerAction
{
	float4x4 mtxView;
	float4x4 mtxViewProj;
	float3 cameraPosition;
	float unused;
};

struct VsPerObject
{
	float4x4 mtxWorld;
};

#endif // SHADER_V_CONSTANTS_H
