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
	, m_orthoHeight(0.f)
	, m_isOrtho(false)
{
	SetAspectRatio(1.f);
}

void Camera::SetAsPerspective(const Vec3& pos, const Vec3& dir, float fovY, float aspectRatio, float nearDist, float farDist)
{
	m_isOrtho = false;
	m_position = pos;
	m_direction = dir;
	m_pitchYawRoll = Vec3::kZero;
	m_fovY = fovY;
	m_nearDist = nearDist;
	m_farDist = farDist;
	SetAspectRatio(aspectRatio);
}

void Camera::SetAsOrtho(const Vec3& pos, const Vec3& dir, float orthoHeight, float nearDist, float farDist)
{
	m_isOrtho = true;
	m_position = pos;
	m_direction = dir;
	m_pitchYawRoll = Vec3::kZero;
	m_orthoHeight = orthoHeight;
	m_nearDist = nearDist;
	m_farDist = farDist;
	UpdateProjection();
}

void Camera::SetAsCubemapFace(const Vec3& pos, const CubemapFace eFace, float nearDist, float farDist)
{
	static const Vec3 faceDirs[6] = {
		Vec3::kUnitX,
		-Vec3::kUnitX,
		Vec3::kUnitY,
		-Vec3::kUnitY,
		Vec3::kUnitZ,
		-Vec3::kUnitZ
	};
	float angle = Maths::DegToRad(90.f);
	SetAsPerspective(pos, faceDirs[(int)eFace], angle, 1.f, nearDist, farDist);
}

void Camera::SetAsSphere(const Vec3& pos, float nearDist, float farDist)
{
	SetAsPerspective(pos, Vec3::kUnitZ, Maths::kTwoPi, 1.f, nearDist, farDist);
}

void Camera::UpdateProjection()
{
	if (m_isOrtho)
	{
		m_projMatrix = Matrix44OrthographicLH(m_orthoHeight * m_aspectRatio, m_orthoHeight, m_nearDist, m_farDist);
	}
	else
	{
		m_projMatrix = Matrix44PerspectiveFovLH(m_fovY, m_aspectRatio, m_nearDist, m_farDist);
	}
}

void Camera::SetAspectRatio(float aspectRatio)
{
	m_aspectRatio = aspectRatio;
	UpdateProjection();
}

void Camera::GetMatrices(Matrix44& view, Matrix44& proj) const
{
	Vec3 upDir = Vec3::kUnitY;
	if (m_direction.y > 0.99f)
		upDir = -Vec3::kUnitZ;
	else if (m_direction.y < -0.99f)
		upDir = Vec3::kUnitZ;

	view = Matrix44LookToLH(m_position, m_direction, upDir);
	proj = m_projMatrix;
}

void Camera::UpdateFrustum(void)
{
	// Half distances
	float hNear, wNear;
	float hFar, wFar;
	if (m_isOrtho)
	{
		hFar = hNear = (m_orthoHeight * 0.5f);
		wFar = wNear = (m_orthoHeight * 0.5f) * m_aspectRatio;
	}
	else
	{
		float heightOverDist = tanf(m_fovY / 2.f);
		hNear = heightOverDist * m_nearDist;
		wNear = hNear * m_aspectRatio;
		hFar = heightOverDist * m_farDist;
		wFar = hFar * m_aspectRatio;
	}

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

bool Camera::CanSee(const Vec3& pos, float radius) const
{
	if (m_fovY > Maths::kPi)
	{
		// Spherical camera.  Used by cubemap captures.
		Vec3 diff = pos - m_position;
		float distSqr = Vec3Dot(diff, diff) - (radius * radius);
		float maxDist = radius + m_farDist;
		return distSqr <= (maxDist * maxDist);
	}
	else
	{
		for (int i = 0; i < 6; ++i)
		{
			float dist = m_frustum.planes[i].Distance(pos);
			if (dist < -radius)
				return false;
		}
	}

	return true;
}
