#pragma once

#include "Input.h"

class Camera;

class CameraInputContext : public IInputContext
{
public:
	CameraInputContext();

	void Update(float dt);

	void GainedFocus();
	void LostFocus();

	void HandleKeyDown(int key, bool down);
	void HandleMouseDown(int button, bool down, int x, int y);
	void HandleMouseMove(int x, int y, int dx, int dy);

	bool WantsKeyDownRepeat();

	void SetCamera(Camera* pCamera);
private:
	Camera* m_pCamera;
	float m_moveSpeed;
	float m_maxMoveSpeed;
	float m_accel;
};

inline bool CameraInputContext::WantsKeyDownRepeat()
{ 
	return false; 
}

inline void CameraInputContext::SetCamera(Camera* pCamera)
{
	m_pCamera = pCamera;
}