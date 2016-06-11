#include "Precompiled.h"
#include "RdrDrawOp.h"

namespace
{
	typedef FreeList<RdrDrawOp, 16 * 1024> RdrDrawOpList;
	RdrDrawOpList s_drawOpList;

	std::vector<const RdrDrawOp*> s_queuedFrees;
}

RdrDrawOp* RdrDrawOp::Allocate()
{
	RdrDrawOp* pDrawOp = s_drawOpList.alloc();
	memset(pDrawOp, 0, sizeof(RdrDrawOp));
	return pDrawOp;
}

void RdrDrawOp::QueueRelease(const RdrDrawOp* pDrawOp)
{
	s_queuedFrees.push_back(pDrawOp);
}

void RdrDrawOp::ProcessReleases()
{
	for (int i = (int)s_queuedFrees.size() - 1; i >= 0; --i)
	{
		s_drawOpList.release(s_queuedFrees[i]);
	}
	s_queuedFrees.clear();
}
