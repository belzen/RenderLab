#pragma once

#include <DirectXMath.h>
#include "Rotation.h"

struct Quaternion : public DirectX::XMFLOAT4
{
	static const Quaternion kIdentity;

	Quaternion();
	Quaternion(float x, float y, float z, float w);
	Quaternion(const float* pFloats);
	Quaternion(const DirectX::XMVECTOR& vec);

	static Quaternion FromRotation(const Rotation& r);
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

inline Quaternion Quaternion::FromRotation(const Rotation& r)
{
	return DirectX::XMQuaternionRotationRollPitchYaw(r.pitch, r.yaw, r.roll);
}
