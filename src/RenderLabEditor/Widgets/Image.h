#pragma once

#include "Widget.h"
#include "IconLibrary.h"

class Image : public Widget
{
public:
	static Image* Create(const Widget& rParent, int x, int y, int width, int height, Icon icon);

private:
	Image(const Widget& rParent, int x, int y, int width, int height, Icon icon);
	Image(const Image&);

	virtual ~Image();

private:
	HBITMAP m_hBitmap;
};