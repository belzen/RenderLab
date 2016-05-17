#pragma once
#include "UtilsLib/ThreadMutex.h"

template <class T_object, uint16 T_nMaxEntries>
class FreeList
{
public:
	typedef uint16 Handle;

	FreeList() 
		: m_nextId(1), m_numFree(0)
	{
		memset(m_inUse, 0, sizeof(m_inUse));
	}

	inline T_object* alloc()
	{
		Handle id = 0;
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
		AutoScopedLock lock(m_mutex);
		return alloc();
	}

	inline void releaseId(Handle id)
	{
		m_inUse[id] = false;
		m_freeIdStack[m_numFree++] = id;
	}

	inline void releaseIdSafe(Handle id)
	{
		AutoScopedLock lock(m_mutex);
		releaseId(id);
	}

	inline void release(T_object* pObj)
	{
		releaseId(getId(pObj));
	}

	inline void releaseSafe(T_object* pObj)
	{
		AutoScopedLock lock(m_mutex);
		releaseId(getId(pObj));
	}

	inline T_object* get(Handle id)
	{
		assert(m_inUse[id]);
		return &m_objects[id];
	}

	inline const T_object* get(Handle id) const
	{
		assert(m_inUse[id]);
		return &m_objects[id];
	}

	inline Handle getId(T_object* pObj)
	{
		return (Handle)(pObj - m_objects);
	}

	inline void AcquireLock()
	{
		m_mutex.Lock();
	}

	inline void ReleaseLock()
	{
		m_mutex.Unlock();
	}

private:
	T_object m_objects[T_nMaxEntries];
	Handle m_freeIdStack[T_nMaxEntries];
	bool m_inUse[T_nMaxEntries];
	ThreadMutex m_mutex;
	Handle m_nextId;
	Handle m_numFree;
};