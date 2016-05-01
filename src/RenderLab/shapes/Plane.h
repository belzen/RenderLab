#pragma once

struct Plane
{
	Plane();
	Plane(const Vec3& pt1, const Vec3& pt2, const Vec3& pt3);

	float Distance(const Vec3& pos) const;

	Vec3 m_normal;
	float m_distance;
};

inline Plane::Plane()
	: m_normal(0.f, 1.f, 0.0f)
	, m_distance(0.f)
{

}

inline Plane::Plane(const Vec3& pt1, const Vec3& pt2, const Vec3& pt3)
{
	Vec3 edge21 = pt1 - pt2;
	Vec3 edge31 = pt1 - pt3;

	m_normal = Vec3Normalize(Vec3Cross(edge21, edge31));
	m_distance = -Vec3Dot(m_normal, pt1);
}

inline float Plane::Distance(const Vec3& pos) const
{
	float d = Vec3Dot(m_normal, pos);
	return d + m_distance;
}