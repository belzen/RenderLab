#pragma once
#include "../Types.h"

struct UVec2 : public DirectX::XMUINT2
{
	UVec2();
	UVec2(uint x, uint y);
	UVec2(const uint* pInts);
	UVec2(const DirectX::XMVECTOR& vec);
};

inline UVec2::UVec2()
	: XMUINT2(0, 0)
{
}

inline UVec2::UVec2(uint x, uint y)
	: XMUINT2(x, y)
{

}

inline UVec2::UVec2(const uint* pInts)
	: XMUINT2(pInts)
{

}

inline UVec2::UVec2(const DirectX::XMVECTOR& vec)
{
	DirectX::XMStoreUInt2(this, vec);
}