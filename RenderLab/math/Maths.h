#pragma once

#include <math.h>
#include <float.h>
#include <DirectXMath.h>
#include "Vec2.h"
#include "Vec3.h"
#include "Vec4.h"
#include "Quaternion.h"
#include "Matrix44.h"
#include "Matrix33.h"

#include "Vec3Math.h"

using std::min;
using std::max;

namespace Maths
{
	const float kPi = DirectX::XM_PI;

	inline float DegToRad(float deg)
	{
		return deg * kPi / 180.f;
	}
}
