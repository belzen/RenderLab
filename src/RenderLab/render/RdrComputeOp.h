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

	RdrResourceHandle hResources[4];
	uint resourceCount;

	RdrSamplerState samplers[4];
	uint samplerCount;

	RdrResourceHandle hWritableResources[8];
	uint writableResourceCount;

	RdrConstantBufferHandle hCsConstants;
};
