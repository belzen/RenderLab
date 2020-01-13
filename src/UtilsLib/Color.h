#pragma once

#include <algorithm>

typedef unsigned char uint8;

struct Color
{
	static const Color kWhite;
	static const Color kBlack;

	Color() : r(0), g(0), b(0), a(1) {}
	Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}
	const float* asFloat4() const { return &r; };

	float r, g, b, a;
};

inline bool operator == (const Color& lhs, const Color& rhs)
{
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}

inline bool ColorNearEqual(const Color& lhs, const Color& rhs)
{
	const float kTolerance = 0.00001f;
	return abs(lhs.r - rhs.r) < kTolerance && abs(lhs.g - rhs.g) < kTolerance && abs(lhs.b - rhs.b) < kTolerance && abs(lhs.a - rhs.a) < kTolerance;
}

struct Color32
{
	Color32() : r(0), g(0), b(0), a(255) {}
	Color32(uint8 r_, uint8 g_, uint8 b_, uint8 a_) : r(r_), g(g_), b(b_), a(a_) {}

	void fromColor(const Color& c)
	{
		r = uint8(c.r * 255);
		g = uint8(c.g * 255);
		b = uint8(c.b * 255);
		a = uint8(c.a * 255);
	}

	uint8 r, g, b, a;
};

inline bool operator == (const Color32& lhs, const Color32& rhs)
{
	return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b && lhs.a == rhs.a;
}
