#pragma once

#include <windows.h>

class ThreadMutex
{
public:
	ThreadMutex()
	{
		InitializeCriticalSection(&m_lock);
	}

	~ThreadMutex()
	{
		DeleteCriticalSection(&m_lock);
	}

	inline void Lock()
	{
		EnterCriticalSection(&m_lock);
	}

	inline void Unlock()
	{
		LeaveCriticalSection(&m_lock);
	}

private:
	CRITICAL_SECTION m_lock;
};

class AutoScopedLock
{
public:
	AutoScopedLock(ThreadMutex& rLock)
		: m_rLock(rLock)
	{
		m_rLock.Lock();
	}

	~AutoScopedLock()
	{
		m_rLock.Unlock();
	}

private:
	ThreadMutex& m_rLock;
};