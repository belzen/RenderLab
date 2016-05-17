#pragma once

namespace Timer
{
	typedef int Handle;

	Handle Create(void);
	void Release(Handle hTimer);

	float GetElapsedSeconds(Handle hTimer);
	float GetElapsedSecondsAndReset(Handle hTimer);
	void Reset(Handle hTimer);
}
