#pragma once

#include "Math.h"

enum class CubemapFace
{
	PositiveX,
	NegativeX,
	PositiveY,
	NegativeY,
	PositiveZ,
	NegativeZ,

	Count
};

class Camera
{
public:
	Camera();

	void SetAsPerspective(const Vec3& pos, const Vec3& dir, float fovY, float aspectRatio, float nearDist, float farDist);
	void SetAsOrtho(const Vec3& pos, const Vec3& dir, float width, float height, float nearDist, float farDist);
	void SetAsCubemapFace(const Vec3& pos, const CubemapFace eFace, float nearDist, float farDist);
	void SetAsSphere(const Vec3& pos, float nearDist, float farDist);

	void UpdateProjection();
	void GetMatrices(Matrix44& view, Matrix44& proj) const;

	float GetAspectRatio() const;
	void SetAspectRatio(float aspectRatio);

	float GetFieldOfViewY() const;

	float GetNearDist() const;
	float GetFarDist() const;

	void SetPosition(const Vec3& pos);
	const Vec3& GetPosition(void) const;

	const Vec3& GetPitchYawRoll() const;
	void SetPitchYawRoll(const Vec3& pitchYawRoll);

	const Vec3& GetDirection(void) const;

	void UpdateFrustum(void);
	bool CanSee(const Vec3& pos, float radius) const;

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

inline float Camera::GetAspectRatio() const
{ 
	return m_aspectRatio; 
}

inline float Camera::GetFieldOfViewY() const
{ 
	return m_fovY; 
}

inline float Camera::GetNearDist() const
{ 
	return m_nearDist; 
}

inline float Camera::GetFarDist() const
{ 
	return m_farDist; 
}

inline void Camera::SetPosition(const Vec3& pos)
{ 
	m_position = pos; 
}

inline const Vec3& Camera::GetPosition(void) const
{ 
	return m_position; 
}

inline const Vec3& Camera::GetPitchYawRoll() const
{ 
	return m_pitchYawRoll; 
}

inline const Vec3& Camera::GetDirection(void) const
{ 
	return m_direction; 
}
