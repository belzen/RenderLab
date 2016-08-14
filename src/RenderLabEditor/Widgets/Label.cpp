#include "Precompiled.h"
#include "Label.h"

Label* Label::Create(const Widget& rParent, int x, int y, int width, int height, const char* text)
{
	return new Label(rParent, x, y, width, height, text);
}

Label::Label(const Widget& rParent, int x, int y, int width, int height, const char* text)
{
	CreateRootWidgetWindow(rParent.GetWindowHandle(), "Static", x, y, width, height);
	SetText(text);
}

Label::~Label()
{

}

void Label::SetText(const char* text)
{
	SetWindowTextA(GetWindowHandle(), text);
}
