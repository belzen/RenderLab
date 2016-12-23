#include "Precompiled.h"
#include "Image.h"
#include "Label.h"

Image* Image::Create(const Widget& rParent, int x, int y, int width, int height, const char* text)
{
	return new Image(rParent, x, y, width, height, text);
}

Image::Image(const Widget& rParent, int x, int y, int width, int height, const char* text)
	: Widget(x, y, width, height, &rParent)
{
	m_pLabel = Label::Create(*this, x, y, width, height, text);
}

Image::~Image()
{

}

void Image::SetText(const char* text)
{
	m_pLabel->SetText(text);
}

