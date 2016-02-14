#include "Precompiled.h"
#include "Camera.h"
#include "input/Input.h"

Camera::Camera()
	: m_position(0.f, 0.f, 0.f)
	, m_direction(0.f, 0.f, 1.f)
	, m_pitchYawRoll(0.f, 0.f, 0.f)
	, m_fovY(60.f / 180.f * Maths::kPi)
	, m_nearDist(0.01f)
	, m_farDist(1000.f)
{
	SetAspectRatio(1.f);
}

void Camera::SetAspectRatio(float aspectRatio)
{
	m_aspectRatio = aspectRatio;
	m_projMatrix = Matrix44PerspectiveFovLH(m_fovY, m_aspectRatio, m_nearDist, m_farDist);
}

void Camera::GetMatrices(Matrix44& view, Matrix44& proj)
{
	view = Matrix44LookToLH(m_position, m_direction, Vec3::kUnitY);
	proj = m_projMatrix;
}

void Camera::UpdateFrustum(void)
{
	// Half distances
	float hNear = tanf(m_fovY / 2.f) * m_nearDist;
	float wNear = hNear * m_aspectRatio;

	float hFar = tanf(m_fovY / 2.f) * m_farDist;
	float wFar = hFar * m_aspectRatio;

	Vec3 pos = m_position;
	Vec3 dir = m_direction;

	Vec3 up = Vec3::kUnitY;
	Vec3 right = Vec3Normalize( Vec3Cross(up, dir) );
	up = Vec3Normalize( Vec3Cross(dir, right) );

	Vec3 nc = pos + dir * m_nearDist;

	Vec3 ntl = nc - wNear * right + hNear * up;
	Vec3 ntr = nc + wNear * right + hNear * up;
	Vec3 nbl = nc - wNear * right - hNear * up;
	Vec3 nbr = nc + wNear * right - hNear * up;

	Vec3 fc = pos + dir * m_farDist;

	Vec3 ftl = fc - wFar * right + hFar * up;
	Vec3 ftr = fc + wFar * right + hFar * up;
	Vec3 fbl = fc - wFar * right - hFar * up;
	Vec3 fbr = fc + wFar * right - hFar * up;

	m_frustum.planes[0] = Plane(ntr, ntl, nbl); // Near
	m_frustum.planes[1] = Plane(ftl, ftr, fbl); // Far
	m_frustum.planes[2] = Plane(ntl, ftl, fbl); // Left
	m_frustum.planes[3] = Plane(ftr, ntr, nbr); // Right
	m_frustum.planes[4] = Plane(ntl, ntr, ftl); // Top
	m_frustum.planes[5] = Plane(nbr, nbl, fbl); // Bottom
}

bool Camera::CanSee(const Vec3 pos, float radius) const
{
	for (int i = 0; i < 6; ++i)
	{
		float dist = m_frustum.planes[i].Distance(pos);
		if (dist < -radius)
			return false;
	}

	return true;
}
