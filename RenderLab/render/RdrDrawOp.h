#pragma once
#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"

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

	RdrShaderHandle hVertexShaders[kRdrShaderMode_Count];
	RdrShaderHandle hPixelShaders[kRdrShaderMode_Count];
	RdrShaderHandle hGeometryShaders[kRdrShaderMode_Count];

	RdrGeoHandle hGeo;
	bool bFreeGeo;

	RdrShaderHandle hComputeShader;
	int computeThreads[3];

	bool needsLighting;
};