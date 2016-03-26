#include "Precompiled.h"
#include "RdrDrawOp.h"
#include "FreeList.h"

typedef FreeList<RdrDrawOp, 16 * 1024> RdrDrawOpList;
RdrDrawOpList g_drawOpList;

RdrDrawOp* RdrDrawOp::Allocate()
{
	RdrDrawOp* pDrawOp = g_drawOpList.alloc();
	memset(pDrawOp, 0, sizeof(RdrDrawOp));
	return pDrawOp;
}

void RdrDrawOp::Release(RdrDrawOp* pDrawOp)
{
	g_drawOpList.release(pDrawOp);
}