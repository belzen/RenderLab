#pragma once

#include <algorithm>
#include "Vec2.h"

inline bool Vec2NearEqual(const Vec2& lhs, const Vec2& rhs)
{
	const float kTolerance = 0.00001f;
	return abs(lhs.x - rhs.x) < kTolerance && abs(lhs.y - rhs.y) < kTolerance;
}

inline Vec2 Vec2Normalize(const Vec2& dir)
{
	float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
	return Vec2(dir.x / len, dir.y / len);
}

inline float Vec2Dot(const Vec2& lhs, const Vec2& rhs)
{
	return lhs.x * rhs.x + lhs.y * rhs.y;
}

inline float Vec2Length(const Vec2& vec)
{
	return sqrtf(vec.x * vec.x + vec.y * vec.y);
}
