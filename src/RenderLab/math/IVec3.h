#pragma once

struct IVec3 : public DirectX::XMINT3
{
	static const IVec3 kZero;

	IVec3();
	IVec3(int x, int y, int z);
	IVec3(const int* pInts);
	IVec3(const DirectX::XMVECTOR& vec);
};

inline IVec3::IVec3()
	: XMINT3(0, 0, 0)
{
}

inline IVec3::IVec3(int x, int y, int z)
	: XMINT3(x, y, z)
{

}

inline IVec3::IVec3(const int* pInts)
	: XMINT3(pInts)
{

}

inline IVec3::IVec3(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreSInt3(this, vec);
}