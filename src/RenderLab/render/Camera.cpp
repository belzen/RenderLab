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

void Camera::GetViewMatrix(Matrix44& rOutView) const
{
	Vec3 upDir = Vec3::kUnitY;
	if (m_direction.y > 0.99f)
		upDir = -Vec3::kUnitZ;
	else if (m_direction.y < -0.99f)
		upDir = Vec3::kUnitZ;

	rOutView = Matrix44LookToLH(m_position, m_direction, upDir);
}

void Camera::GetMatrices(Matrix44& view, Matrix44& proj) const
{
	GetViewMatrix(view);
	proj = m_projMatrix;
}

void Camera::UpdateFrustum(void)
{
	Matrix44 viewMtx;
	Matrix44 projMtx;
	GetMatrices(viewMtx, projMtx);

	Matrix44 viewProjMtx = Matrix44Multiply(viewMtx, projMtx);

	for (uint i = 0; i < 2; ++i)
	{
		m_frustum.planes[2 * i] = Plane(
			viewProjMtx._14 - viewProjMtx.m[0][i],
			viewProjMtx._24 - viewProjMtx.m[1][i],
			viewProjMtx._34 - viewProjMtx.m[2][i],
			viewProjMtx._44 - viewProjMtx.m[3][i]);

		m_frustum.planes[2 * i + 1] = Plane(
			viewProjMtx._14 + viewProjMtx.m[0][i],
			viewProjMtx._24 + viewProjMtx.m[1][i],
			viewProjMtx._34 + viewProjMtx.m[2][i],
			viewProjMtx._44 + viewProjMtx.m[3][i]);
	}

	// Near plane
	m_frustum.planes[4] = Plane( viewProjMtx._13, viewProjMtx._23, viewProjMtx._33, viewProjMtx._43);

	// Far plane
	m_frustum.planes[5] = Plane(
		viewProjMtx._14 - viewProjMtx._13,
		viewProjMtx._24 - viewProjMtx._23,
		viewProjMtx._34 - viewProjMtx._33,
		viewProjMtx._44 - viewProjMtx._43);
		
}

Quad Camera::GetFrustumQuad(float depth) const
{
	float width, height;

	if (m_isOrtho)
	{
		height = (m_orthoHeight * 0.5f);
	}
	else
	{
		float heightOverDist = tanf(m_fovY * 0.5f);
		height = heightOverDist * depth;
	}

	width = height * m_aspectRatio;

	Vec3 right = Vec3Normalize(Vec3Cross(Vec3::kUnitY, m_direction));
	Vec3 up = Vec3Normalize(Vec3Cross(m_direction, right));
	Vec3 center = m_position + m_direction * depth;

	Quad quad;
	quad.topLeft = center + up * height - right * width;
	quad.topRight = center + up * height + right * width;
	quad.bottomLeft = center - up * height - right * width;
	quad.bottomRight = center - up * height + right * width;
	return quad;
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

Vec3 Camera::CalcRayDirection(float x, float y) const
{
	float ndcX = 2.f * x - 1.f;
	float ndcY = 1.f - 2.f * y;

	// Convert from NDC [-1, 1] space to view space.
	Vec3 ray = Vec3::kZero;
	ray.x = ndcX * m_nearDist / m_projMatrix._11;
	ray.y = ndcY * m_nearDist / m_projMatrix._22;
	ray.z = m_nearDist;

	// Transform ray into world space
	Matrix44 mtxView;
	GetViewMatrix(mtxView);
	return Vec3Normalize(Vec3TransformNormal(Vec3Normalize(ray), Matrix44Inverse(mtxView)));
}
