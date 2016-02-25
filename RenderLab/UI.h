#pragma once

#include "math/Vec3.h"

namespace UI
{
	enum AlignmentFlags
	{
		kAlign_Default = 0x0, // Top-left
		kAlign_CenterH = 0x1,
		kAlign_CenterV = 0x2,
		kAlign_Right   = 0x4,
		kAlign_Top     = 0x6,
		kAlign_Bottom  = 0x8,
	};

	enum Units
	{
		kUnits_Pixels,
		kUnits_Percentage
	};

	struct Coord
	{
		Coord(float x) : val(x) {}
		Coord(float x, Units u) : val(x), units(u) {}

		float val;
		Units units;
	};

	struct Position
	{
		Position(const Coord& x, const Coord& y, float depth, AlignmentFlags align)
			: x(x), y(y), alignment(align), depth(depth) {}

		Coord x;
		Coord y;
		AlignmentFlags alignment;
		float depth;
	};

	Vec3 PosToScreenSpace(const UI::Position& uiPos, const Vec2& elemSize, const Vec2& viewportSize);
}
