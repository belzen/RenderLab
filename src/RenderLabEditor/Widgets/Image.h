#pragma once

#include "Widget.h"

class Label;

class Image : public Widget
{
public:
	static Image* Create(const Widget& rParent, int x, int y, int width, int height, const char* text);

	void SetText(const char* text);

private:
	Image(const Widget& rParent, int x, int y, int width, int height, const char* text);
	Image(const Image&);

	virtual ~Image();

private:
	Label* m_pLabel;
};