#pragma once
#include "RdrDeviceTypes.h"
#include "RdrShaders.h"
#include "RdrResource.h"

struct RdrComputeOp
{
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
