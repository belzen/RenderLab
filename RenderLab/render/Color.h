#pragma once

struct Color
{
	static const Color kWhite;

	Color() : r(0), g(0), b(0), a(1) {}
	Color(float r, float g, float b, float a) : r(r), g(g), b(b), a(a) {}

	float r, g, b, a;
};
