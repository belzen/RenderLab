#pragma once

#include <windows.h>
#include "Widget.h"

class Panel : public Widget
{
public:
	static Panel* Create(const Widget& rParent, int x, int y, int width, int height);

private:
	Panel(const Widget& rParent, int x, int y, int width, int height);
	Panel(const Panel&);

	virtual ~Panel();

private:
};