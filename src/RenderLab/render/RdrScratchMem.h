#pragma once

// Allocator for transient frame memory.
// This is implemented as two linear allocators that are flipped between frames.
// As such, memory does not need to be explicitly freed, but it becomes invalid every frame.
// Should really only be used for queuing render thread commands.
namespace RdrScratchMem
{
	void* Alloc(const uint size);
	void* AllocAligned(const uint size, const uint alignment);

	void FlipState();
}
