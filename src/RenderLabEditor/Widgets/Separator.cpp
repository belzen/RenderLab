#include "Precompiled.h"
#include "Separator.h"

namespace
{
	static const char* kSeparatorClassName = "Separator";
}

Separator* Separator::Create(const Widget& rParent, int x, int y, int width, int height)
{
	static bool bRegisteredClass = false;
	if (!bRegisteredClass)
	{
		WNDCLASSEXA wc = { 0 };
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = Widget::WndProc;
		wc.hInstance = ::GetModuleHandle(NULL);
		wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)CreateSolidBrush(RGB(0, 0, 0));
		wc.lpszClassName = kSeparatorClassName;

		::RegisterClassExA(&wc);
		bRegisteredClass = true;
	}

	return new Separator(rParent, x, y, width, height);
}

Separator::Separator(const Widget& rParent, int x, int y, int width, int height)
	: Widget(x, y, width, height, &rParent, 0, kSeparatorClassName)
{
}

Separator::~Separator()
{

}
