#pragma once

#include "RdrResource.h"

struct RdrResourceReadbackRequest
{
	RdrResourceHandle hSrcResource;
	RdrResourceHandle hDstResource;
	RdrBox srcRegion;
	char* pData;
	uint dataSize;
	uint frameCount;
	bool bComplete;
};
typedef FreeList<RdrResourceReadbackRequest, 32 * 3> RdrResourceReadbackRequestList;
typedef RdrResourceReadbackRequestList::Handle RdrResourceReadbackRequestHandle;