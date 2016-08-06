#pragma once
#include "RdrDeviceTypes.h"
#include "RdrShaders.h"
#include "RdrResource.h"

struct RdrComputeOp
{
	static RdrComputeOp* Allocate();
	static void QueueRelease(const RdrComputeOp* pDrawOp);
	static void ProcessReleases();

	RdrComputeShader shader;
	uint threads[3];

	RdrResourceHandle hViews[8];
	uint viewCount;

	RdrResourceHandle hTextures[4];
	uint texCount;

	RdrConstantBufferHandle hCsConstants;
};
