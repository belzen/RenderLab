#include "Precompiled.h"
#include "RdrFrameState.h"

void RdrAction::Reset()
{
	for (int i = 0; i < kRdrBucketType_Count; ++i)
		buckets[i].clear();
	memset(passes, 0, sizeof(passes));
	memset(&lightParams, 0, sizeof(lightParams));
}