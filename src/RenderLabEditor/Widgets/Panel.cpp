#include "Precompiled.h"
#include "Panel.h"
#include "WindowBase.h"

Panel* Panel::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new Panel(rParent, x, y, width, height);
}

Panel::Panel(const Widget& rParent, int x, int y, int width, int height)
{
	const char* kWndClassName = "Panel";
	static bool s_bRegisteredClass = false;
	if (!s_bRegisteredClass)
	{
		RegisterWindowClass(kWndClassName, DefWindowProc);
		s_bRegisteredClass = true;
	}

	CreateRootWidgetWindow(rParent.GetWindowHandle(), kWndClassName, x, y, width, height);
}

Panel::~Panel()
{

}
