#pragma once
#include "UtilsLib/Array.h"
#include "RdrDeviceTypes.h"
#include "RdrShaders.h"
#include "RdrResource.h"

struct RdrComputeOp
{
	static uint getThreadGroupCount(uint dataSize, uint workSize);

	RdrComputeShader shader;
	uint threads[3];

	Array<RdrResourceHandle, 8>       ahResources;
	Array<RdrSamplerState, 4>         aSamplers;
	Array<RdrResourceHandle, 4>       ahWritableResources;
	Array<RdrConstantBufferHandle, 4> ahConstantBuffers;
};

inline uint RdrComputeOp::getThreadGroupCount(uint dataSize, uint workSize)
{
	uint count = dataSize / workSize;
	if (dataSize % workSize != 0)
		++count;
	return count;
}
