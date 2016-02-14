#pragma once
#include "Types.h"
#include <assert.h>

template <class T_object, uint T_nMaxEntries>
class FreeList
{
public:
	FreeList() 
		: m_nextId(1), m_numFree(0)
	{
		memset(m_inUse, 0, sizeof(m_inUse));
	}

	T_object* alloc()
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

	void releaseId(uint id)
	{
		m_inUse[id] = false;
		m_freeIdStack[m_numFree++] = id;
	}

	void release(T_object* pObj)
	{
		releaseId(getId(pObj));
	}

	T_object* get(uint id)
	{
		assert(m_inUse[id]);
		return &m_objects[id];
	}

	uint getId(T_object* pObj)
	{
		return pObj - m_objects;
	}

private:
	T_object m_objects[T_nMaxEntries];
	uint m_freeIdStack[T_nMaxEntries];
	bool m_inUse[T_nMaxEntries];
	uint m_nextId;
	uint m_numFree;
};