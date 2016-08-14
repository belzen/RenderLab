#pragma once

#include "Widget.h"

class TextBox : public Widget
{
public:
	static TextBox* Create(const Widget& rParent, int x, int y, int width, int height);

private:
	TextBox(const Widget& rParent, int x, int y, int width, int height);
	TextBox(const TextBox&);

	virtual ~TextBox();
};