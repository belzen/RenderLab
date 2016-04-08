#pragma once
#include "Types.h"
#include <assert.h>
#include "ThreadLock.h"

template <class T_object, uint16 T_nMaxEntries>
class FreeList
{
public:
	FreeList() 
		: m_nextId(1), m_numFree(0)
	{
		memset(m_inUse, 0, sizeof(m_inUse));
	}

	inline T_object* alloc()
	{
		uint id = 0;
		if (m_numFree > 0)
		{
			id = m_freeIdStack[--m_numFree];
		}
		else
		{
			id = m_nextId++;
		}

		assert(id < T_nMaxEntries);

		m_inUse[id] = true;
		return &m_objects[id];
	}

	inline T_object* allocSafe()
	{
		AutoScopedLock lock(m_lock);
		return alloc();
	}

	inline void releaseId(uint16 id)
	{
		m_inUse[id] = false;
		m_freeIdStack[m_numFree++] = id;
	}

	inline void releaseIdSafe(uint16 id)
	{
		AutoScopedLock lock(m_lock);
		releaseId(id);
	}

	inline void release(T_object* pObj)
	{
		releaseId(getId(pObj));
	}

	inline void releaseSafe(T_object* pObj)
	{
		AutoScopedLock lock(m_lock);
		releaseId(getId(pObj));
	}

	inline T_object* get(uint16 id)
	{
		assert(m_inUse[id]);
		return &m_objects[id];
	}

	inline const T_object* get(uint16 id) const
	{
		assert(m_inUse[id]);
		return &m_objects[id];
	}

	inline uint getId(T_object* pObj)
	{
		return (uint)(pObj - m_objects);
	}

	inline void AcquireLock()
	{
		m_lock.Lock();
	}

	inline void ReleaseLock()
	{
		m_lock.Unlock();
	}

private:
	T_object m_objects[T_nMaxEntries];
	uint m_freeIdStack[T_nMaxEntries];
	bool m_inUse[T_nMaxEntries];
	ThreadLock m_lock;
	uint m_nextId;
	uint m_numFree;
};