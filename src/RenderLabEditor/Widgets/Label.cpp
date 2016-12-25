#include "Precompiled.h"
#include "Label.h"

Label* Label::Create(const Widget& rParent, int x, int y, int width, int height, const char* text)
{
	return new Label(rParent, x, y, width, height, text);
}

Label::Label(const Widget& rParent, int x, int y, int width, int height, const char* text)
	: Widget(x, y, width, height, &rParent, "Static")
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
