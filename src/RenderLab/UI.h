#pragma once

#include "math/Vec3.h"

namespace UI
{
	enum class AlignmentFlags
	{
		None    = 0x0,
		CenterH = 0x1,
		CenterV = 0x2,
		Left    = 0x4,
		Right   = 0x8,
		Top     = 0x10,
		Bottom  = 0x20,

		Default = Top | Left
	};
	ENUM_FLAGS(AlignmentFlags)

	enum class Units
	{
		Pixels,
		Percentage
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
