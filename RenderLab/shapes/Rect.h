#pragma once

struct Rect
{
	Rect() 
		: top(0.f), left(0.f), width(0.f), height(0.f) {}
	Rect(float l, float t, float w, float h)
		: top(t), left(l), width(w), height(h) {}

	float top;
	float left;
	float width;
	float height;
};
