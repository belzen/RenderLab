#include "Precompiled.h"
#include "RdrTransientHeap.h"

// todo: Custom transient heap
//		 - Linear allocator?  Fast, but would need two large buffers to flip frame.
void* RdrTransientHeap::Alloc(const uint size)
{
	return (void*)new char[size];
}

void* RdrTransientHeap::AllocAligned(const uint size, const uint alignment)
{
	// todo: actually align...
	return (void*)new char[size];
}

void RdrTransientHeap::Free(const void* pData)
{
	delete pData;
}

void RdrTransientHeap::SafeFree(const void* pData)
{
	if (pData)
	{
		delete pData;
	}
}
