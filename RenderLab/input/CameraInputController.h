#pragma once

#include "IInputController.h"

class Camera;

class CameraInputController : public IInputController
{
public:
	CameraInputController();

	void SetCamera(Camera* pCamera);
	void Update(float dt);

private:
	Camera* m_pCamera;
	float m_moveSpeed;
	float m_maxMoveSpeed;
	float m_accel;
};

inline void CameraInputController::SetCamera(Camera* pCamera)
{
	m_pCamera = pCamera;
}