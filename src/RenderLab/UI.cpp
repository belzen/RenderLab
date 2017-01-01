#include "Precompiled.h"
#include "UI.h"

namespace
{
	Widget* s_pDraggedWidget = nullptr;
}

Vec3 UI::PosToScreenSpace(const UI::Position& uiPos, const Vec2& elemSize, const Vec2& viewportSize)
{
	Vec3 pos(uiPos.x.val, -uiPos.y.val, uiPos.depth);

	if (uiPos.x.units == UI::Units::Percentage)
		pos.x *= viewportSize.x;

	if (uiPos.y.units == UI::Units::Percentage)
		pos.y *= viewportSize.y;

	if ((uiPos.alignment & UI::AlignmentFlags::Right) != UI::AlignmentFlags::None)
		pos.x -= elemSize.x;

	if ((uiPos.alignment & UI::AlignmentFlags::Bottom) != UI::AlignmentFlags::None)
		pos.y -= elemSize.y;

	pos.x -= viewportSize.x * 0.5f;
	pos.y += viewportSize.y * 0.5f;

	return pos;
}

void UI::SetDraggedWidget(Widget* pWidget)
{
	s_pDraggedWidget = pWidget;
}

Widget* UI::GetDraggedWidget()
{
	return s_pDraggedWidget;
}
