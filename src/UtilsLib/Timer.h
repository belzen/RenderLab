#pragma once

namespace Timer
{
	typedef int Handle;

	Handle Create(void);
	void Release(Handle hTimer);

	double GetElapsedSeconds(Handle hTimer);
	double GetElapsedSecondsAndReset(Handle hTimer);

	double GetElapsedMilliseconds(Handle hTimer);
	double GetElapsedMillisecondsAndReset(Handle hTimer);

	void Reset(Handle hTimer);
}
