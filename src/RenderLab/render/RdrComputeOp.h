#pragma once
#include "UtilsLib/Array.h"
#include "RdrDeviceTypes.h"
#include "RdrShaders.h"
#include "RdrResource.h"

struct RdrComputeOp
{
	static uint getThreadGroupCount(uint dataSize, uint workSize);

	RdrPipelineState pipelineState;
	uint threads[3];

	const RdrDescriptorTable* pResourceDescriptorTable;
	const RdrDescriptorTable* pSamplerDescriptorTable;
	const RdrDescriptorTable* pUnorderedAccessDescriptorTable;
	const RdrDescriptorTable* pConstantsDescriptorTable;

	RdrDebugBackpointer debug;
};

inline uint RdrComputeOp::getThreadGroupCount(uint dataSize, uint workSize)
{
	uint count = dataSize / workSize;
	if (dataSize % workSize != 0)
		++count;
	return count;
}
