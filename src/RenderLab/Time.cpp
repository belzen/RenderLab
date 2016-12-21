#include "Precompiled.h"
#include "Time.h"

Time Time::s_time;

Time::Time()
	: m_frameCount(0)
	, m_timer(0.f)
	, m_fps(0.f)
	, m_frameTime(0.f)
	, m_unscaledFrameTime(0.f)
	, m_timeScale(1.f)
{

}

void Time::SetTimeScale(float scale)
{
	s_time.m_timeScale = scale;
}

void Time::Update(float dt)
{
	s_time.m_timer += dt;
	++s_time.m_frameCount;

	if (s_time.m_timer > 1.f)
	{
		s_time.m_fps = s_time.m_frameCount / s_time.m_timer;
		s_time.m_timer = 0.f;
		s_time.m_frameCount = 0;
	}

	s_time.m_unscaledFrameTime = dt;
	s_time.m_frameTime = dt * s_time.m_timeScale;
}
