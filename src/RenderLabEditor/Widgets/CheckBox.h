#pragma once
#include "Widget.h"

class CheckBox : public Widget
{
public:
	typedef void(*ToggledFunc)(CheckBox* pCheckBox, bool newState);

	static CheckBox* Create(const Widget& rParent, int x, int y, int width, int height,
		ToggledFunc toggledCallback);

private:
	CheckBox(const Widget& rParent, int x, int y, int width, int height,
		ToggledFunc toggledCallback);
	CheckBox(const CheckBox&);

	virtual ~CheckBox();

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	HWND m_hCheckBox;
	ToggledFunc m_toggledCallback;
};