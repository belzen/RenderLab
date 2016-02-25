#include "Precompiled.h"
#include "FrameTimer.h"

FrameTimer::FrameTimer()
	: m_frameCount(0)
	, m_timer(0.f)
	, m_fps(0.f)
{

}

void FrameTimer::Update(float dt)
{
	m_timer += dt;
	++m_frameCount;

	if (m_timer > 1.f)
	{
		m_fps = m_frameCount / m_timer;
		m_timer = 0.f;
		m_frameCount = 0;
	}
}
