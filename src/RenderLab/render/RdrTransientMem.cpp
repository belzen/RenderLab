#include "Precompiled.h"
#include "RdrTransientMem.h"
#include "LinearAllocator.h"

namespace
{
	const uint kFrameMemSize = 32 * 1024 * 1024; // 32 MB per frame, 64 MB total.
	LinearAllocator<kFrameMemSize> s_mem[2];
	uint s_frame = 0;
}

void RdrTransientMem::FlipState()
{
	s_frame = !s_frame;
	s_mem[s_frame].Reset();
}

void* RdrTransientMem::Alloc(const uint size)
{
	return s_mem[s_frame].Alloc(size);
}

void* RdrTransientMem::AllocAligned(const uint size, const uint alignment)
{
	return s_mem[s_frame].Alloc(size, alignment);
}
