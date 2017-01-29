#pragma once

struct IVec3
{
	int x, y, z;

	//////////////////////////////////////////////////////////////////////////
	static const IVec3 kZero;

	IVec3();
	IVec3(int x, int y, int z);
	IVec3(const int* pInts);
	IVec3(const DirectX::XMVECTOR& vec);
};

inline IVec3::IVec3()
	: x(0), y(0), z(0)
{
}

inline IVec3::IVec3(int vx, int vy, int vz)
	: x(vx), y(vy), z(vz)
{

}

inline IVec3::IVec3(const int* pInts)
	: x(pInts[0]), y(pInts[1]), z(pInts[2])
{

}

inline IVec3::IVec3(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreSInt3((DirectX::XMINT3*)this, vec);
}
