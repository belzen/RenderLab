#pragma once

namespace RdrTransientHeap
{
	void* Alloc(const uint size);
	void* AllocAligned(const uint size, const uint alignment);
	void Free(const void* pData);
	void SafeFree(const void* pData);
}
