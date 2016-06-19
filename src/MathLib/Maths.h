#pragma once

#include <math.h>
#include <float.h>
#include "Vec2.h"
#include "Vec3.h"
#include "Vec4.h"
#include "UVec2.h"
#include "IVec3.h"
#include "Quaternion.h"
#include "Matrix44.h"
#include "Matrix33.h"

#include "Vec3Math.h"
#include "../Types.h"

namespace Maths
{
	const float kPi = DirectX::XM_PI;
	const float kTwoPi = DirectX::XM_2PI;

	inline float DegToRad(float deg)
	{
		return deg * kPi / 180.f;
	}

	// Input data MUST be 16 byte aligned.
	inline Vec4 convertHalfToSinglePrecision4(float16* pHalf)
	{
		__m128i halfs = _mm_load_si128((__m128i*)pHalf);
		__m128 floats = _mm_cvtph_ps(halfs);

		Vec4 res;
		_mm_storeu_ps(&res.x, floats);

		return res;
	}
}
