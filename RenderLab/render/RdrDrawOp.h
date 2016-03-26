#pragma once
#include "RdrEnums.h"
#include "RdrHandles.h"
#include "RdrDeviceObjects.h"

#define MAX_TEXTURES_PER_DRAW 16

struct RdrDrawOp
{
	static RdrDrawOp* Allocate();
	static void Release(RdrDrawOp* pDrawOp);

	Vec3 position;

	RdrSamplerState samplers[MAX_TEXTURES_PER_DRAW];
	RdrResourceHandle hTextures[MAX_TEXTURES_PER_DRAW];
	uint texCount;

	// UAVs
	RdrResourceHandle hViews[8];
	uint viewCount;

	Vec4 constants[16];
	uint numConstants;

	ShaderHandle hVertexShaders[kRdrShaderMode_Count];
	ShaderHandle hPixelShaders[kRdrShaderMode_Count];

	RdrGeoHandle hGeo;
	bool bFreeGeo;

	ShaderHandle hComputeShader;
	int computeThreads[3];

	bool needsLighting;
};