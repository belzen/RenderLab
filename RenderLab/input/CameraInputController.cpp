#include "Precompiled.h"
#include "CameraInputController.h"
#include "Input.h"
#include "render/Camera.h"

CameraInputController::CameraInputController()
	: m_pCamera(nullptr)
	, m_moveSpeed(0.0f)
	, m_maxMoveSpeed(0.1f)
	, m_accel(0.01f)
{

}

void CameraInputController::Update(float dt)
{
	float right = 0.f;
	float up = 0.f;
	float forward = 0.f;

	// Rotation
	if (Input::IsMouseDown(1))
	{
		// todo: fix rotations at straight up/down
		static const float kPixelToAngle = Maths::kPi / (180.f * 10.f);
		int moveX, moveY;
		Input::GetMouseMove(moveX, moveY);

		Vec3 pitchYawRoll = m_pCamera->GetPitchYawRoll();

		pitchYawRoll.x += moveY * kPixelToAngle;
		pitchYawRoll.y += moveX * kPixelToAngle;
		m_pCamera->SetPitchYawRoll(pitchYawRoll);
	}

	// Translation
	if (Input::IsKeyDown(KEY_W))
		forward = 1.f;
	else if (Input::IsKeyDown(KEY_S))
		forward = -1.f;

	if (Input::IsKeyDown(KEY_A))
		right = -1.f;
	else if (Input::IsKeyDown(KEY_D))
		right = 1.f;

	if (Input::IsKeyDown(KEY_Q))
		up = 1.f;
	else if (Input::IsKeyDown(KEY_Z))
		up = -1.f;

	if (fabs(right) + fabs(up) + fabs(forward) != 0.f)
	{
		m_moveSpeed += m_accel * dt;
		if (m_moveSpeed > m_maxMoveSpeed)
			m_moveSpeed = m_maxMoveSpeed;
	}
	else
	{
		m_moveSpeed = 0.0f;
	}

	Vec3 pos = m_pCamera->GetPosition();
	Vec3 dir = m_pCamera->GetDirection();

	Matrix44 rotationMatrix = Matrix44LookToLH(pos, dir, Vec3::kUnitY);
	rotationMatrix = Matrix44Inverse(rotationMatrix);
	Vec3 moveVec(right * m_moveSpeed, up * m_moveSpeed, forward * m_moveSpeed);
	moveVec = Vec3TransformNormal(moveVec, rotationMatrix);

	m_pCamera->SetPosition(pos + moveVec);
}
