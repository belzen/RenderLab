#include "Precompiled.h"
#include "RdrDrawOp.h"

namespace
{
	typedef FreeList<RdrDrawOp, 16 * 1024> RdrDrawOpList;
	RdrDrawOpList s_drawOpList;
}

RdrDrawOp* RdrDrawOp::Allocate()
{
	RdrDrawOp* pDrawOp = s_drawOpList.alloc();
	memset(pDrawOp, 0, sizeof(RdrDrawOp));
	return pDrawOp;
}

void RdrDrawOp::Release(RdrDrawOp* pDrawOp)
{
	s_drawOpList.release(pDrawOp);
}