#include "Precompiled.h"
#include "RdrFrameState.h"

void RdrAction::Reset()
{
	for (int i = 0; i < (int)RdrBucketType::Count; ++i)
		buckets[i].clear();
	for (int i = 0; i < MAX_SHADOW_MAPS_PER_FRAME; ++i)
	{
		shadowPasses[i].drawOps.clear();
		memset(&shadowPasses[i].passData, 0, sizeof(shadowPasses[i].passData));
	}

	memset(passes, 0, sizeof(passes));
	memset(&lightParams, 0, sizeof(lightParams));
	shadowPassCount = 0;
}