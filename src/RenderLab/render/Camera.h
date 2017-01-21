#pragma once

#include "Math.h"
#include "shapes/Quad.h"
#include "shapes/Plane.h"

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
	void SetAsOrtho(const Vec3& pos, const Vec3& dir, float orthoHeight, float nearDist, float farDist);
	void SetAsCubemapFace(const Vec3& pos, const CubemapFace eFace, float nearDist, float farDist);
	void SetAsSphere(const Vec3& pos, float nearDist, float farDist);

	void UpdateProjection();
	void GetMatrices(Matrix44& view, Matrix44& proj) const;
	void GetViewMatrix(Matrix44& rOutView) const;

	float GetAspectRatio() const;
	void SetAspectRatio(float aspectRatio);

	float GetFieldOfViewY() const;

	float GetNearDist() const;
	float GetFarDist() const;

	void SetPosition(const Vec3& pos);
	const Vec3& GetPosition(void) const;

	const Rotation& GetRotation() const;
	void SetRotation(const Rotation& r);

	const Vec3& GetDirection(void) const;

	void UpdateFrustum(void);
	Quad GetFrustumQuad(float depth) const;

	bool CanSee(const Vec3& pos, float radius) const;

	// Calculate direction of ray from a point on the near plane.
	Vec3 CalcRayDirection(float x, float y) const;

private:
	struct Frustum
	{
		Plane planes[6];
	};

	Matrix44 m_projMatrix;
	Vec3 m_position;
	Vec3 m_direction;
	Rotation m_rotation;
	
	Frustum m_frustum;

	float m_fovY;
	float m_nearDist;
	float m_farDist;
	float m_aspectRatio; // Width / Height

	float m_orthoHeight;
	bool m_isOrtho;
};

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

inline void Camera::SetRotation(const Rotation& r)
{
	m_rotation = r;
	m_direction = Vec3Rotate(Vec3::kUnitZ, Quaternion::FromRotation(r));
}

inline const Rotation& Camera::GetRotation() const
{ 
	return m_rotation; 
}

inline const Vec3& Camera::GetDirection(void) const
{ 
	return m_direction; 
}
