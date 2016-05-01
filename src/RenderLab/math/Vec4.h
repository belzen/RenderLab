#pragma once

struct Vec4 : public DirectX::XMFLOAT4
{
	Vec4();
	Vec4(float x, float y, float z, float w);
	Vec4(const float* pFloats);
	Vec4(const DirectX::XMVECTOR& vec);
};

inline Vec4::Vec4()
	: XMFLOAT4(0.f, 0.f, 0.f, 0.f)
{

}

inline Vec4::Vec4(float x, float y, float z, float w)
	: XMFLOAT4(x, y, z, w)
{
}

inline Vec4::Vec4(const float* pFloats)
	: XMFLOAT4(pFloats)
{

}

inline Vec4::Vec4(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreFloat4(this, vec);
}