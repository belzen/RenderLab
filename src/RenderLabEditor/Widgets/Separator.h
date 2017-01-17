#pragma once

#include "Widget.h"

class Separator : public Widget
{
public:
	static Separator* Create(const Widget& rParent, int x, int y, int width, int height);

private:
	Separator(const Widget& rParent, int x, int y, int width, int height);
	Separator(const Separator&);

	virtual ~Separator();
};