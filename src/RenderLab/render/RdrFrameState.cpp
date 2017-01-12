#include "Precompiled.h"
#include "RdrFrameState.h"

#define ENABLE_DRAWOP_VALIDATION 0

namespace
{
	void validateDrawOp(const RdrDrawOp* pDrawOp)
	{
		assert(pDrawOp->hGeo);
	}
}

void RdrDrawBuckets::AddDrawOp(const RdrDrawOp* pDrawOp, RdrBucketType eBucket)
{
#if ENABLE_DRAWOP_VALIDATION
	validateDrawOp(pDrawOp);
#endif
	RdrDrawBucketEntry entry(pDrawOp);
	m_drawOpBuckets[(int)eBucket].push_back(entry);
}

void RdrDrawBuckets::AddComputeOp(const RdrComputeOp* pComputeOp, RdrPass ePass)
{
	m_computeOpBuckets[(int)ePass].push_back(pComputeOp);
}

void RdrDrawBuckets::SortDrawOps(RdrBucketType eBucketType)
{
	RdrDrawOpBucket& rBucket = m_drawOpBuckets[(int)eBucketType];
	std::sort(rBucket.begin(), rBucket.end(), RdrDrawBucketEntry::SortCompare);
}

const RdrDrawOpBucket& RdrDrawBuckets::GetDrawOpBucket(RdrBucketType eBucketType) const
{
	return m_drawOpBuckets[(int)eBucketType];
}

const RdrComputeOpBucket& RdrDrawBuckets::GetComputeOpBucket(RdrPass ePass) const
{
	return m_computeOpBuckets[(int)ePass];
}

void RdrDrawBuckets::Clear()
{
	for (RdrDrawOpBucket& rBucket : m_drawOpBuckets)
	{
		rBucket.clear();
	}

	for (RdrComputeOpBucket& rBucket : m_computeOpBuckets)
	{
		rBucket.clear();
	}
}

void RdrAction::Reset()
{
	opBuckets.Clear();
	lights.Clear();

	memset(&lightParams, 0, sizeof(lightParams));

	for (int i = 0; i < MAX_SHADOW_MAPS_PER_FRAME; ++i)
	{
		shadowPasses[i].buckets.Clear();
		memset(&shadowPasses[i].passData, 0, sizeof(shadowPasses[i].passData));
	}

	for (int i = 0; i < (int)RdrPass::Count; ++i)
	{
		RdrPassData& rPass = passes[i];
		memset(rPass.ahRenderTargets, 0, sizeof(rPass.ahRenderTargets));
		rPass.hDepthTarget = 0;
		rPass.viewport = Rect(0, 0, 0, 0);
		rPass.shaderMode = RdrShaderMode::Normal;
		rPass.depthTestMode = RdrDepthTestMode::None;
		rPass.bAlphaBlend = false;
		rPass.bEnabled = false;
		rPass.bClearRenderTargets = false;
		rPass.bClearDepthTarget = false;
		rPass.bIsCubeMapCapture = false;
	}

	shadowPassCount = 0;
}