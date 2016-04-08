#pragma once

class ThreadLock
{
public:
	ThreadLock()
	{
		InitializeCriticalSection(&m_lock);
	}

	~ThreadLock()
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
	AutoScopedLock(ThreadLock& rLock)
		: m_rLock(rLock)
	{
		m_rLock.Lock();
	}

	~AutoScopedLock()
	{
		m_rLock.Unlock();
	}

private:
	ThreadLock& m_rLock;
};