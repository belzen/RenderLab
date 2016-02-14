#pragma once

#include "Math.h"

class Camera
{
public:
	Camera();

	void GetMatrices(Matrix44& view, Matrix44& proj);

	float GetAspectRatio() { return m_aspectRatio; }
	void SetAspectRatio(float aspectRatio);

	float GetFieldOfViewY() { return m_fovY; }

	float GetNearDist() { return m_nearDist; }
	float GetFarDist() { return m_farDist; }

	void SetPosition(Vec3 pos) { m_position = pos; }
	const Vec3& Camera::GetPosition(void) const { return m_position; }

	const Vec3& GetPitchYawRoll() const { return m_pitchYawRoll; }
	void SetPitchYawRoll(const Vec3& pitchYawRoll);

	const Vec3& GetDirection(void) const { return m_direction; }

	void UpdateFrustum(void);
	bool CanSee(const Vec3 pos, float radius) const;

private:
	struct Frustum
	{
		Plane planes[6];
	};

	Matrix44 m_projMatrix;
	Vec3 m_position;
	Vec3 m_direction;
	Vec3 m_pitchYawRoll;
	
	Frustum m_frustum;

	float m_fovY;
	float m_nearDist;
	float m_farDist;
	float m_aspectRatio;
};

inline void Camera::SetPitchYawRoll(const Vec3& pitchYawRoll)
{
	m_pitchYawRoll = pitchYawRoll;
	Matrix44 rotation = Matrix44RotationYawPitchRoll(pitchYawRoll.y, pitchYawRoll.x, pitchYawRoll.z);
	m_direction = Vec3TransformNormal(Vec3::kUnitZ, rotation);
}