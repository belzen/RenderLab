#include "Precompiled.h"
#include "RdrComputeOp.h"

namespace
{
	typedef FreeList<RdrComputeOp, 1024> RdrComputeOpList;
	RdrComputeOpList s_computeOpList;

	std::vector<const RdrComputeOp*> s_queuedFrees;
}

RdrComputeOp* RdrComputeOp::Allocate()
{
	RdrComputeOp* pComputeOp = s_computeOpList.alloc();
	memset(pComputeOp, 0, sizeof(RdrComputeOp));
	return pComputeOp;
}

void RdrComputeOp::QueueRelease(const RdrComputeOp* pDrawOp)
{
	s_queuedFrees.push_back(pDrawOp);
}

void RdrComputeOp::ProcessReleases()
{
	for (int i = (int)s_queuedFrees.size() - 1; i >= 0; --i)
	{
		s_computeOpList.release(s_queuedFrees[i]);
	}
	s_queuedFrees.clear();
}
