#pragma once

struct Quaternion : public DirectX::XMFLOAT4
{
	static const Quaternion kIdentity;

	Quaternion();
	Quaternion(float x, float y, float z, float w);
	Quaternion(const float* pFloats);
	Quaternion(const DirectX::XMVECTOR& vec);
};

inline Quaternion::Quaternion()
	: XMFLOAT4(0.f, 0.f, 0.f, 0.f)
{

}

inline Quaternion::Quaternion(float x, float y, float z, float w)
	: XMFLOAT4(x, y, z, w)
{
}

inline Quaternion::Quaternion(const float* pFloats)
	: XMFLOAT4(pFloats)
{

}

inline Quaternion::Quaternion(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreFloat4(this, vec);
}

inline Quaternion QuaternionPitchYawRoll(float pitch, float yaw, float roll)
{
	return DirectX::XMQuaternionRotationRollPitchYaw(pitch, yaw, roll);
}
