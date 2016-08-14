#pragma once

#include "Widget.h"

class Label : public Widget
{
public:
	static Label* Create(const Widget& rParent, int x, int y, int width, int height, const char* text);

	void SetText(const char* text);

private:
	Label(const Widget& rParent, int x, int y, int width, int height, const char* text);
	Label(const Label&);

	virtual ~Label();
};