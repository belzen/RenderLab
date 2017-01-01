#pragma once

#include "Input.h"

class Camera;

class CameraInputContext : public IInputContext
{
public:
	CameraInputContext();

	void Update(const InputManager& rInputManager);

	void GainedFocus();
	void LostFocus();

	void HandleChar(char c);
	void HandleKeyDown(int key, bool down);
	void HandleMouseDown(int button, bool down, int x, int y);
	void HandleMouseMove(int x, int y, int dx, int dy);

	void SetCamera(Camera* pCamera);
private:
	Camera* m_pCamera;
	float m_moveSpeed;
	float m_maxMoveSpeed;
	float m_accel;
};

inline void CameraInputContext::SetCamera(Camera* pCamera)
{
	m_pCamera = pCamera;
}