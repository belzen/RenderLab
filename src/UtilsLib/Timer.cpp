#include "Timer.h"
#include <wtypesbase.h>
#include <WinBase.h>
#include <assert.h>

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

	assert(false);
	return -1;
}

void Timer::Release(Timer::Handle hTimer)
{
	s_timers[hTimer].inUse = false;
}

float Timer::GetElapsedSeconds(Timer::Handle hTimer)
{
	LARGE_INTEGER end;
	LARGE_INTEGER elapsed;

	QueryPerformanceCounter(&end);
	elapsed.QuadPart = end.QuadPart - s_timers[hTimer].start.QuadPart;

	return elapsed.QuadPart / (float)s_timers[hTimer].frequency.QuadPart;
}

float Timer::GetElapsedSecondsAndReset(Timer::Handle hTimer)
{
	LARGE_INTEGER end;
	LARGE_INTEGER elapsed;

	QueryPerformanceCounter(&end);
	elapsed.QuadPart = end.QuadPart - s_timers[hTimer].start.QuadPart;
	
	float fElapsed = elapsed.QuadPart / (float)s_timers[hTimer].frequency.QuadPart;
	s_timers[hTimer].start = end;

	return fElapsed;
}
