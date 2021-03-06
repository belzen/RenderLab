#pragma once

#include "Quaternion.h"
#include "Vec3.h"

struct Matrix44 : public DirectX::XMFLOAT4X4
{
	static const Matrix44 kIdentity;

	Matrix44();
	Matrix44(const float* pArray);
	Matrix44(const DirectX::XMMATRIX& mtx);
	Matrix44(const Vec3& translation);

	const Vec3& GetTranslation() const;
	void SetTranslation(const Vec3& pos);
};

inline Matrix44::Matrix44() 
{ 
	*this = kIdentity; 
}

inline Matrix44::Matrix44(const float* pArray)
	: XMFLOAT4X4(pArray)
{

}

inline Matrix44::Matrix44(const DirectX::XMMATRIX& mtx)
{
	DirectX::XMStoreFloat4x4(this, mtx);
}

inline Matrix44::Matrix44(const Vec3& translation)
{
	*this = kIdentity;
	m[3][0] = translation.x;
	m[3][1] = translation.y;
	m[3][2] = translation.z;
}

inline const Vec3& Matrix44::GetTranslation() const
{ 
	return m[3]; 
}

inline void Matrix44::SetTranslation(const Vec3& pos)
{
	m[3][0] = pos.x;
	m[3][1] = pos.y;
	m[3][2] = pos.z;
}

inline Matrix44 Matrix44Translation(const Vec3& translation)
{
	return DirectX::XMMatrixTranslation(translation.x, translation.y, translation.z);
}

inline Matrix44 Matrix44Transformation(
	const Vec3& scalingOrigin,
	const Quaternion& scalingOrientationQuaternion,
	const Vec3& scaling,
	const Vec3& rotationOrigin,
	const Quaternion& rotationQuaternion,
	const Vec3& translation )
{
	DirectX::XMVECTOR vScalingOrigin = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&scalingOrigin);
	DirectX::XMVECTOR vScalingOrientationQuaternion = DirectX::XMLoadFloat4(&scalingOrientationQuaternion);
	DirectX::XMVECTOR vScaling = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&scaling);
	DirectX::XMVECTOR vRotationOrigin = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&rotationOrigin);
	DirectX::XMVECTOR vRotationQuaternion = DirectX::XMLoadFloat4((DirectX::XMFLOAT4*)&rotationQuaternion);
	DirectX::XMVECTOR vTranslation = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&translation);
	return DirectX::XMMatrixTransformation(
		vScalingOrigin,
		vScalingOrientationQuaternion,
		vScaling,
		vRotationOrigin,
		vRotationQuaternion,
		vTranslation);
}

inline Matrix44 Matrix44Transpose(const Matrix44& src)
{
	DirectX::XMMATRIX result(
		src._11, src._12, src._13, src._14,
		src._21, src._22, src._23, src._24,
		src._31, src._32, src._33, src._34,
		src._41, src._42, src._43, src._44 );

	return DirectX::XMMatrixTranspose(result);
}

inline float Matrix44Determinant(const Matrix44& mtx)
{
	DirectX::XMMATRIX m = DirectX::XMLoadFloat4x4(&mtx);
	return DirectX::XMMatrixDeterminant(m).m128_f32[0];
}

inline Matrix44 Matrix44LookToLH(const Vec3& eyePos, const Vec3& eyeDir, const Vec3& upDir)
{
	DirectX::XMVECTOR vEyePos = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&eyePos);
	DirectX::XMVECTOR vEyeDir = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&eyeDir);
	DirectX::XMVECTOR vUpDir = DirectX::XMLoadFloat3((DirectX::XMFLOAT3*)&upDir);
	return DirectX::XMMatrixLookToLH(vEyePos, vEyeDir, vUpDir);
}

inline Matrix44 Matrix44Inverse(const Matrix44& src)
{
	DirectX::XMMATRIX mtx = DirectX::XMLoadFloat4x4(&src);
	return DirectX::XMMatrixInverse(nullptr, mtx);
}

inline Matrix44 Matrix44Multiply(const Matrix44& lhs, const Matrix44& rhs)
{
	DirectX::XMMATRIX mtx1 = DirectX::XMLoadFloat4x4(&lhs);
	DirectX::XMMATRIX mtx2 = DirectX::XMLoadFloat4x4(&rhs);
	return DirectX::XMMatrixMultiply(mtx1, mtx2);
}

inline Matrix44 Matrix44OrthographicLH(float width, float height, float nearZ, float farZ)
{
	return DirectX::XMMatrixOrthographicLH(width, height, nearZ, farZ);
}

inline Matrix44 Matrix44PerspectiveFovLH(float fovY, float aspectRatio, float nearZ, float farZ)
{
	return DirectX::XMMatrixPerspectiveFovLH(fovY, aspectRatio, nearZ, farZ);
}

inline Matrix44 Matrix44RotationPitchYawRoll(float pitch, float yaw, float roll)
{
	return DirectX::XMMatrixRotationRollPitchYaw(pitch, yaw, roll);
}
