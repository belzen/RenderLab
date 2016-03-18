#pragma once

#include "Math.h"

class Camera
{
public:
	Camera();

	void SetAsPerspective(const Vec3& pos, const Vec3& dir, float fovY, float aspectRatio, float nearDist, float farDist);
	void SetAsOrtho(const Vec3& pos, const Vec3& dir, float width, float height, float nearDist, float farDist);

	void UpdateProjection();
	void GetMatrices(Matrix44& view, Matrix44& proj) const;

	float GetAspectRatio() const { return m_aspectRatio; }
	void SetAspectRatio(float aspectRatio);

	float GetFieldOfViewY() const { return m_fovY; }

	float GetNearDist() const { return m_nearDist; }
	float GetFarDist() const { return m_farDist; }

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

	Vec2 m_orthoSize;
	bool m_isOrtho;
};

inline void Camera::SetPitchYawRoll(const Vec3& pitchYawRoll)
{
	m_pitchYawRoll = pitchYawRoll;
	Matrix44 rotation = Matrix44RotationPitchYawRoll(pitchYawRoll.x, pitchYawRoll.y, pitchYawRoll.z);
	m_direction = Vec3TransformNormal(Vec3::kUnitZ, rotation);
}