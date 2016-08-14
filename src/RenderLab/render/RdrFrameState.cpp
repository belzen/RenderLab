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

	for (int i = 0; i < (int)RdrPass::Count; ++i)
	{
		RdrPassData& rPass = passes[i];
		memset(rPass.ahRenderTargets, 0, sizeof(rPass.ahRenderTargets));
		rPass.hDepthTarget = 0;
		rPass.computeOps.clear();
		rPass.viewport = Rect(0, 0, 0, 0);
		rPass.shaderMode = RdrShaderMode::Normal;
		rPass.depthTestMode = RdrDepthTestMode::None;
		rPass.bAlphaBlend = false;
		rPass.bEnabled = false;
		rPass.bClearRenderTargets = false;
		rPass.bClearDepthTarget = false;
		rPass.bIsCubeMapCapture = false;
	}

	memset(&lightParams, 0, sizeof(lightParams));
	shadowPassCount = 0;
}