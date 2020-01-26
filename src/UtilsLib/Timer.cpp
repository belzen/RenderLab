#include "Timer.h"
#include <wtypesbase.h>
#include <WinBase.h>
#include "Error.h"

struct TimerObj
{
	LARGE_INTEGER start;
	LARGE_INTEGER frequency;
	bool inUse;
};

static const int kMaxTimers = 32;
static TimerObj s_timers[kMaxTimers];

Timer::Handle Timer::Create(void)
{
	for (int i = 0; i < kMaxTimers; ++i)
	{
		if (!s_timers[i].inUse)
		{
			QueryPerformanceFrequency(&s_timers[i].frequency);
			QueryPerformanceCounter(&s_timers[i].start);
			s_timers[i].inUse = true;
			return i;
		}
	}

	Assert(false);
	return -1;
}

void Timer::Release(Timer::Handle hTimer)
{
	s_timers[hTimer].inUse = false;
}

double Timer::GetElapsedSeconds(Timer::Handle hTimer)
{
	LARGE_INTEGER end;
	LARGE_INTEGER elapsed;

	QueryPerformanceCounter(&end);
	elapsed.QuadPart = end.QuadPart - s_timers[hTimer].start.QuadPart;

	return elapsed.QuadPart / (double)s_timers[hTimer].frequency.QuadPart;
}

double Timer::GetElapsedSecondsAndReset(Timer::Handle hTimer)
{
	LARGE_INTEGER end;
	LARGE_INTEGER elapsed;

	QueryPerformanceCounter(&end);
	elapsed.QuadPart = end.QuadPart - s_timers[hTimer].start.QuadPart;
	
	double fElapsed = elapsed.QuadPart / (double)s_timers[hTimer].frequency.QuadPart;
	s_timers[hTimer].start = end;

	return fElapsed;
}

double Timer::GetElapsedMilliseconds(Timer::Handle hTimer) 
{
	return GetElapsedSeconds(hTimer) * 1000; 
}

double Timer::GetElapsedMillisecondsAndReset(Timer::Handle hTimer) 
{ 
	return GetElapsedSecondsAndReset(hTimer) * 1000; 
}

void Timer::Reset(Timer::Handle hTimer)
{
	GetElapsedSecondsAndReset(hTimer);
}
