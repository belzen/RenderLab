#pragma once

struct Vec2
{
	float x, y;

	//////////////////////////////////////////////////////////////////////////
	static const Vec2 kZero;
	static const Vec2 kOne;

	Vec2();
	Vec2(float px, float py);

	float& operator[] (int idx);
	const float& operator[] (int idx) const;
};

inline Vec2::Vec2()
	: x(0.0f), y(0.0f) 
{
}

inline Vec2::Vec2(float px, float py)
	: x(px), y(py) 
{
}

inline float& Vec2::operator[] (int component)
{ 
	return *(&x + component);
}

inline const float& Vec2::operator[] (int component) const 
{
	return *(&x + component);
}

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

inline Vec2 operator * (float scalar, const Vec2& rhs)
{
	return Vec2(rhs.x * scalar, rhs.y * scalar);
}

inline Vec2& operator *= (Vec2& lhs, float scalar)
{
	lhs.x *= scalar;
	lhs.y *= scalar;
	return lhs;
}

inline Vec2 operator / (const Vec2& lhs, const Vec2& rhs)
{
	return Vec2(lhs.x / rhs.x, lhs.y / rhs.y);
}

inline Vec2 operator / (const Vec2& lhs, const float rhs)
{
	return Vec2(lhs.x / rhs, lhs.y / rhs);
}

inline bool operator == (const Vec2& lhs, const Vec2& rhs)
{
	return lhs.x == rhs.x && lhs.y == rhs.y;
}
