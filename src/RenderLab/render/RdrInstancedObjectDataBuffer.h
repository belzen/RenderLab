#pragma once

#include "RdrResource.h"
#include "RdrShaderConstants.h"

typedef uint16 RdrInstancedObjectDataId;

namespace RdrInstancedObjectDataBuffer
{
	RdrInstancedObjectDataId AllocEntry();
	void ReleaseEntry(RdrInstancedObjectDataId id);
	VsPerObject* GetEntry(RdrInstancedObjectDataId id);
	
	RdrResourceHandle GetResourceHandle();
	void UpdateBuffer(Renderer& rRenderer);

	void FlipState();
}