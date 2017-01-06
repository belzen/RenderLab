#include "Precompiled.h"
#include "Label.h"

Label* Label::Create(const Widget& rParent, int x, int y, int width, int height, const char* text, TextAlignment alignment)
{
	return new Label(rParent, x, y, width, height, text, alignment);
}

Label::Label(const Widget& rParent, int x, int y, int width, int height, const char* text, TextAlignment alignment)
	: Widget(x, y, width, height, &rParent, (DWORD)alignment, "Static")
{
	SetText(text);
}

Label::~Label()
{

}

void Label::SetText(const char* text)
{
	m_text = text ? text : "";
	SetWindowTextA(GetWindowHandle(), text);
}

const std::string& Label::GetText() const
{
	return m_text;
}
