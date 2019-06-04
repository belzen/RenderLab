#pragma once

// Global time management.
class Time
{
public:
	static void Update(float dt);

	// Game's current frames per second.
	static float Fps();

	// Frame time with scaling applied.
	static float FrameTime();

	// Actual frame time.
	static float UnscaledFrameTime();

	// Global time since game started.
	static float GlobalTime();

	// Modify the frame time scaling.
	static void SetTimeScale(float scale);

private:
	Time();
	static Time s_time;

private:
	int m_frameCount;
	float m_globalTime;
	float m_timer;
	float m_fps;

	float m_frameTime;
	float m_unscaledFrameTime;
	float m_timeScale;
};

inline float Time::Fps()
{
	return s_time.m_fps;
}

inline float Time::FrameTime()
{
	return s_time.m_frameTime;
}

inline float Time::UnscaledFrameTime()
{
	return s_time.m_unscaledFrameTime;
}

inline float Time::GlobalTime()
{
	return s_time.m_globalTime;
}