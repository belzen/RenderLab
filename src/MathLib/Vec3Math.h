#pragma once

#include <algorithm>
#include "Vec3.h"
#include "Matrix44.h"

inline Vec3 Vec3Normalize(const Vec3& dir)
{
	float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
	return Vec3(dir.x / len, dir.y / len, dir.z / len);
}

inline Vec3 Vec3Abs(const Vec3& vec)
{
	return Vec3(abs(vec.x), abs(vec.y), abs(vec.z));
}

inline float Vec3Dot(const Vec3& lhs, const Vec3& rhs)
{
	return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
}

inline Vec3 Vec3Cross(const Vec3& lhs, const Vec3& rhs)
{
	return Vec3(
		lhs.y * rhs.z - lhs.z * rhs.y,
		lhs.z * rhs.x - lhs.x * rhs.z,
		lhs.x * rhs.y - lhs.y * rhs.x);
}

inline float Vec3Length(const Vec3& vec)
{
	return sqrtf(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
}

inline Vec3 Vec3TransformNormal(const Vec3& lhs, const Matrix44& rotationMtx)
{
	DirectX::XMVECTOR normal = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&lhs);
	DirectX::XMMATRIX mtx = DirectX::XMLoadFloat4x4(&rotationMtx);
	return DirectX::XMVector3TransformNormal(normal, mtx);
}

inline Vec3 Vec3TransformCoord(const Vec3& lhs, const Matrix44& transformMtx)
{
	DirectX::XMVECTOR pos = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&lhs);
	DirectX::XMMATRIX mtx = DirectX::XMLoadFloat4x4(&transformMtx);
	return DirectX::XMVector3TransformCoord(pos, mtx);
}

inline Vec3 Vec3Rotate(const Vec3& lhs, const Quaternion& rotationQuaternion)
{
	DirectX::XMVECTOR dir = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&lhs);
	DirectX::XMVECTOR q = DirectX::XMLoadFloat4(&rotationQuaternion);
	return DirectX::XMVector3Rotate(dir, q);
}

inline float Vec3MaxComponent(const Vec3& vec)
{
	using std::max;
	return max(max(vec.x, vec.y), vec.z);
}

inline Vec3 Vec3Max(const Vec3& lhs, const Vec3& rhs)
{
	using std::max;
	return Vec3(max(lhs.x, rhs.x), max(lhs.y, rhs.y), max(lhs.z, rhs.z));
}

inline Vec3 Vec3Min(const Vec3& lhs, const Vec3& rhs)
{
	using std::min;
	return Vec3(min(lhs.x, rhs.x), min(lhs.y, rhs.y), min(lhs.z, rhs.z));
}