#include "Precompiled.h"
#include "TextBox.h"

TextBox* TextBox::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new TextBox(rParent, x, y, width, height);
}

TextBox::TextBox(const Widget& rParent, int x, int y, int width, int height)
{
	CreateRootWidgetWindow(rParent.GetWindowHandle(), "Edit", x, y, width, height, 0, WS_EX_CLIENTEDGE);
}

TextBox::~TextBox()
{

}
