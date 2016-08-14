#pragma once
#include "Widget.h"

class Button : public Widget
{
public:
	typedef void(*ClickedFunc)();

	static Button* Create(const Widget& rParent, int x, int y, int width, int height,
		const char* text, ClickedFunc clickedCallback);

private:
	Button(const Widget& rParent, int x, int y, int width, int height, 
		const char* text, ClickedFunc clickedCallback);
	Button(const Button&);

	virtual ~Button();

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	HWND m_hButton;
	ClickedFunc m_clickedCallback;
};