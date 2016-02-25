#pragma once

class FrameTimer
{
public:
	FrameTimer();

	void Update(float dt);

	float GetFps() const { return m_fps; }

private:
	int m_frameCount;
	float m_timer;
	float m_fps;
};
