#pragma once

struct Vec2
{
	Vec2() : x(0.0f), y(0.0f) {}
	Vec2(float px, float py) :x(px), y(py) {}

	float& operator[] (int idx) { return _m[idx]; }
	const float& operator[] (int idx) const { return _m[idx]; }

	union
	{
		struct
		{
			float x;
			float y;
		};
		float _m[2];
	};
};

inline Vec2 operator - (const Vec2& v)
{
	return Vec2(-v.x, -v.y);
}

inline Vec2 operator - (const Vec2& lhs, const Vec2& rhs)
{
	return Vec2(lhs.x - rhs.x, lhs.y - rhs.y);
}

inline Vec2 operator + (const Vec2& lhs, const Vec2& rhs)
{
	return Vec2(lhs.x + rhs.x, lhs.y + rhs.y);
}

inline Vec2 operator * (const Vec2& lhs, const Vec2& rhs)
{
	return Vec2(lhs.x * rhs.x, lhs.y * rhs.y);
}

inline Vec2 operator * (const Vec2& lhs, float scalar)
{
	return Vec2(lhs.x * scalar, lhs.y * scalar);
}

inline Vec2 operator / (const Vec2& lhs, const Vec2& rhs)
{
	return Vec2(lhs.x / rhs.x, lhs.y / rhs.y);
}

inline bool operator == (const Vec2& lhs, const Vec2& rhs)
{
	return lhs.x == rhs.x && lhs.y == rhs.y;
}
