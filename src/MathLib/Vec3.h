#pragma once

#include <DirectXMath.h>

struct Vec3
{
	float x, y, z;

	//////////////////////////////////////////////////////////////////////////
	static const Vec3 kOrigin;
	static const Vec3 kOne;
	static const Vec3 kZero;
	static const Vec3 kUnitX;
	static const Vec3 kUnitY;
	static const Vec3 kUnitZ;

	Vec3();
	Vec3(float x, float y, float z);
	Vec3(const float* pFloats);
	Vec3(const DirectX::XMVECTOR& vec);

	float& operator[](const int component);
	const float& operator[](const int component) const;
};

inline Vec3::Vec3()
	: x(0.f), y(0.f), z(0.f)
{
}

inline Vec3::Vec3(float vx, float vy, float vz)
	: x(vx), y(vy), z(vz)
{

}

inline Vec3::Vec3(const float* pFloats)
	: x(pFloats[0]), y(pFloats[1]), z(pFloats[2])
{

}

inline Vec3::Vec3(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)this, vec);
}

inline Vec3 operator - (const Vec3& v)
{
	return Vec3(-v.x, -v.y, -v.z);
}

inline Vec3 operator - (const Vec3& lhs, const Vec3& rhs)
{
	return Vec3(lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z);
}

inline Vec3 operator + (const Vec3& lhs, const Vec3& rhs)
{
	return Vec3(lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z);
}

inline Vec3& operator += (Vec3& lhs, const Vec3& rhs)
{
	lhs.x += rhs.x;
	lhs.y += rhs.y;
	lhs.z += rhs.z;
	return lhs;
}

inline Vec3 operator * (const Vec3& lhs, const Vec3& rhs)
{
	return Vec3(lhs.x * rhs.x, lhs.y * rhs.y, lhs.z * rhs.z);
}

inline Vec3 operator * (const Vec3& lhs, float scalar)
{
	return Vec3(lhs.x * scalar, lhs.y * scalar, lhs.z * scalar);
}

inline Vec3& operator *= (Vec3& lhs, float scalar)
{
	lhs.x *= scalar;
	lhs.y *= scalar;
	lhs.z *= scalar;
	return lhs;
}

inline Vec3 operator * (float scalar, const Vec3& rhs)
{
	return Vec3(rhs.x * scalar, rhs.y * scalar, rhs.z * scalar);
}

inline Vec3 operator / (const Vec3& lhs, const Vec3& rhs)
{
	return Vec3(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z);
}

inline Vec3 operator / (const Vec3& lhs, float scalar)
{
	return Vec3(lhs.x / scalar, lhs.y / scalar, lhs.z / scalar);
}

inline Vec3& operator /= (Vec3& lhs, float scalar)
{
	lhs.x /= scalar;
	lhs.y /= scalar;
	lhs.z /= scalar;
	return lhs;
}

inline bool operator == (const Vec3& lhs, const Vec3& rhs)
{
	return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

inline bool operator != (const Vec3& lhs, const Vec3& rhs)
{
	return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z;
}

inline float& Vec3::operator[](const int component)
{
	return *(&x + component);
}

inline const float& Vec3::operator[](const int component) const
{
	return *(&x + component);
}