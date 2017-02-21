#pragma once
#include "../Types.h"

struct UVec2
{
	uint x, y;

	//////////////////////////////////////////////////////////////////////////
	UVec2();
	UVec2(uint x, uint y);
	UVec2(const uint* pInts);
};

inline UVec2::UVec2()
	: x(0), y(0)
{
}

inline UVec2::UVec2(uint vx, uint vy)
	: x(vx), y(vy)
{

}

inline UVec2::UVec2(const uint* pInts)
	: x(pInts[0]), y(pInts[1])
{

}
