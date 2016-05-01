#pragma once

struct Vec3 : public DirectX::XMFLOAT3
{
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
};

inline Vec3::Vec3()
	: XMFLOAT3(0.f, 0.f, 0.0f)
{
}

inline Vec3::Vec3(float x, float y, float z)
	: XMFLOAT3(x, y, z)
{

}

inline Vec3::Vec3(const float* pFloats)
	: XMFLOAT3(pFloats)
{

}

inline Vec3::Vec3(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreFloat3(this, vec);
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

inline Vec3 operator * (float scalar, const Vec3& rhs)
{
	return Vec3(rhs.x * scalar, rhs.y * scalar, rhs.z * scalar);
}

inline Vec3 operator / (const Vec3& lhs, const Vec3& rhs)
{
	return Vec3(lhs.x / rhs.x, lhs.y / rhs.y, lhs.z / rhs.z);
}

inline bool operator == (const Vec3& lhs, const Vec3& rhs)
{
	return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z;
}

inline bool operator != (const Vec3& lhs, const Vec3& rhs)
{
	return lhs.x != rhs.x || lhs.y != rhs.y || lhs.z != rhs.z;
}
