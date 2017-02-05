#pragma once

#include <DirectXMath.h>

struct Vec4
{
	float x, y, z, w;

	//////////////////////////////////////////////////////////////////////////
	static const Vec4 kOne;

	Vec4();
	Vec4(float x, float y, float z, float w);
	Vec4(const float* pFloats);
	Vec4(const DirectX::XMVECTOR& vec);
};

inline Vec4::Vec4()
	: x(0.f), y(0.f), z(0.f), w(0.f)
{

}

inline Vec4::Vec4(float vx, float vy, float vz, float vw)
	: x(vx), y(vy), z(vz), w(vw)
{
}

inline Vec4::Vec4(const float* pFloats)
	: x(pFloats[0]), y(pFloats[1]), z(pFloats[2]), w(pFloats[3])
{

}

inline Vec4::Vec4(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)this, vec);
}