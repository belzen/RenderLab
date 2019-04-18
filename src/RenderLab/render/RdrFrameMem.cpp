#include "Precompiled.h"
#include "RdrFrameMem.h"
#include "LinearAllocator.h"
#include "RdrDrawOp.h"
#include "RdrComputeOp.h"

namespace
{
	static const uint kMaxFrames = 2;
	const uint kFrameMemSize = 32 * 1024 * 1024;
	const uint kFrameDrawOps = 16 * 1024;
	const uint kFrameComputeOps = 1 * 1024;

	struct FrameDataSet
	{
		LinearAllocator<kFrameMemSize> genericMem;
		StructLinearAllocator<RdrDrawOp, kFrameDrawOps> drawOps;
		StructLinearAllocator<RdrComputeOp, kFrameComputeOps> computeOps;
	};

	FrameDataSet s_frameMem[kMaxFrames];
	uint s_frame = 0;
}

void RdrFrameMem::FlipState()
{
	s_frame = (s_frame + 1) % kMaxFrames;
	s_frameMem[s_frame].genericMem.Reset();
	s_frameMem[s_frame].drawOps.Reset();
	s_frameMem[s_frame].computeOps.Reset();
}

void* RdrFrameMem::Alloc(const uint size)
{
	return s_frameMem[s_frame].genericMem.Alloc(size);
}

void* RdrFrameMem::AllocAligned(const uint size, const uint alignment)
{
	return s_frameMem[s_frame].genericMem.Alloc(size, alignment);
}

RdrDrawOp* RdrFrameMem::AllocDrawOp(const RdrDebugBackpointer& src)
{
	RdrDrawOp* pOp = s_frameMem[s_frame].drawOps.Alloc();
	pOp->debug = src;
	return pOp;
}

RdrDrawOp* RdrFrameMem::AllocDrawOps(uint16 count, const RdrDebugBackpointer& src)
{
	RdrDrawOp* pOps = s_frameMem[s_frame].drawOps.AllocArray(count);
	for (uint i = 0; i < count; ++i)
	{
		pOps[i].debug = src;
	}

	return pOps;
}

RdrComputeOp* RdrFrameMem::AllocComputeOp(const RdrDebugBackpointer& src)
{
	RdrComputeOp* pOp = s_frameMem[s_frame].computeOps.Alloc();
	pOp->debug = src;
	return pOp;
}