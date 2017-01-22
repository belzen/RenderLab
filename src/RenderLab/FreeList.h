#pragma once
#include "UtilsLib/ThreadMutex.h"

// Macro to easily add all FreeList types as friends to a class.
#define FRIEND_FREELIST template<typename T_object, uint16 T_nMaxEntries> friend class FreeList

template <typename T_object, uint16 T_nMaxEntries>
class FreeList
{
public:
	typedef uint16 Handle;
	static const uint16 kMaxEntries = T_nMaxEntries;

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

	inline void release(const T_object* pObj)
	{
		releaseId(getId(pObj));
	}

	inline void releaseSafe(const T_object* pObj)
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

	inline Handle getId(const T_object* pObj)
	{
		return (Handle)(pObj - m_objects);
	}

	// Note: This isn't accurate.  It returns the highest index that 
	// was ever allocated, not what is currently in use.
	inline Handle getMaxUsedId() const
	{
		return m_nextId;
	}

	inline const T_object* data() const
	{
		return m_objects;
	}

	inline void AcquireLock()
	{
		m_mutex.Lock();
	}

	inline void ReleaseLock()
	{
		m_mutex.Unlock();
	}

public:
	//////////////////////////////////////////////////////////////////////////
	// Iterator
	class Iterator 
	{
	public:
		Iterator(Handle index, FreeList<T_object, T_nMaxEntries>* pList) 
			: m_index(index), m_pList(pList)
		{
		}

		T_object& operator*() 
		{ 
			return m_pList->m_objects[m_index];
		}

		Iterator& operator++() 
		{ 
			while (!m_pList->m_inUse[++m_index] && m_index < m_pList->m_nextId);
			return *this; 
		}

		bool operator!=(const Iterator& iter) const 
		{ 
			return m_index != iter.m_index;
		}

	private:
		Handle m_index;
		FreeList<T_object, T_nMaxEntries>* m_pList;
	};

	Iterator begin()
	{
		Iterator iter(0, this);
		// Step the iterator before returning.  
		// 0 is an invalid ID and the ++ operator will find the first in-use element.
		return ++iter;
	}

	Iterator end()
	{ 
		return Iterator(m_nextId, this); 
	}

private:
	// Disable copy constructor
	FreeList(const FreeList&);

private:
	T_object m_objects[T_nMaxEntries];
	Handle m_freeIdStack[T_nMaxEntries];
	bool m_inUse[T_nMaxEntries];
	ThreadMutex m_mutex;
	Handle m_nextId;
	Handle m_numFree;
};