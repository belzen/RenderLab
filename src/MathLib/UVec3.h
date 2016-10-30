#pragma once
#include "../Types.h"

struct UVec3 : public DirectX::XMUINT3
{
	UVec3();
	UVec3(uint x, uint y, uint z);
	UVec3(const uint* pInts);
	UVec3(const DirectX::XMVECTOR& vec);
};

inline UVec3::UVec3()
	: XMUINT3(0, 0, 0)
{
}

inline UVec3::UVec3(uint x, uint y, uint z)
	: XMUINT3(x, y, z)
{

}

inline UVec3::UVec3(const uint* pInts)
	: XMUINT3(pInts)
{

}

inline UVec3::UVec3(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreUInt3(this, vec);
}