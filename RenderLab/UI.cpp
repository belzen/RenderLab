#include "Precompiled.h"
#include "UI.h"


Vec3 UI::PosToScreenSpace(const UI::Position& uiPos, const Vec2& elemSize, const Vec2& viewportSize)
{
	Vec3 pos(uiPos.x.val, -uiPos.y.val, uiPos.depth);

	if (uiPos.x.units == UI::kUnits_Percentage)
		pos.x *= viewportSize.x;

	if (uiPos.y.units == UI::kUnits_Percentage)
		pos.y *= viewportSize.y;

	if (uiPos.alignment & UI::kAlign_Right)
		pos.x -= elemSize.x;

	if (uiPos.alignment & UI::kAlign_Bottom)
		pos.y -= elemSize.y;

	pos.x -= viewportSize.x * 0.5f;
	pos.y += viewportSize.y * 0.5f;

	return pos;
}
