#pragma once
#include "MathLib/Vec3.h"

struct OBB
{
	Vec3 center;
	Vec3 axes[3];
	Vec3 halfSize;
};
