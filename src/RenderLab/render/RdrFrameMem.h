#pragma once

#include "RdrDebugBackpointer.h"

struct RdrDrawOp;
struct RdrComputeOp;

// Allocator for transient frame memory.
// This is implemented as two linear allocators that are flipped between frames.
// As such, memory does not need to be explicitly freed, but it becomes invalid every frame.
// Should really only be used for queuing render thread commands.
namespace RdrFrameMem
{
	void* Alloc(const uint size);
	void* AllocAligned(const uint size, const uint alignment);

	RdrDrawOp* AllocDrawOp(const RdrDebugBackpointer& src);
	RdrDrawOp* AllocDrawOps(uint16 count, const RdrDebugBackpointer& src);
	RdrComputeOp* AllocComputeOp();

	void FlipState();
}
