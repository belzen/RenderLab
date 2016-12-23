#include "Precompiled.h"
#include "Panel.h"
#include "WindowBase.h"

Panel* Panel::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new Panel(rParent, x, y, width, height);
}

Panel::Panel(const Widget& rParent, int x, int y, int width, int height)
	: Widget(x, y, width, height, &rParent)
{
}

Panel::~Panel()
{

}
