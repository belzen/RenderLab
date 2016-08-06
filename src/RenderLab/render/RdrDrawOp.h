#pragma once
#include "RdrDeviceTypes.h"
#include "RdrGeometry.h"
#include "RdrShaders.h"
#include "RdrMaterial.h"

struct RdrDispatchOp
{
	static RdrDispatchOp* Allocate();
	static void QueueRelease(const RdrDispatchOp* pDrawOp);
	static void ProcessReleases();

	RdrComputeShader shader;
	uint threads[3];

	RdrResourceHandle hViews[8];
	uint viewCount;

	RdrResourceHandle hTextures[4];
	uint texCount;

	RdrConstantBufferHandle hCsConstants;
};

struct RdrDrawOp
{
	static RdrDrawOp* Allocate();
	static void QueueRelease(const RdrDrawOp* pDrawOp);
	static void ProcessReleases();

	RdrConstantBufferHandle hVsConstants;

	RdrInputLayoutHandle hInputLayout;
	RdrVertexShader      vertexShader;

	const RdrMaterial* pMaterial;

	RdrGeoHandle hGeo;
	uint8 bFreeGeo : 1;
	uint8 bHasAlpha : 1;
	uint8 bIsSky : 1;
	uint8 bTempDrawOp : 1;
};