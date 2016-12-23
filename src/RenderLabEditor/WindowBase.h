#pragma once

#include <windows.h>
#include "input\Input.h"
#include "Widgets/Widget.h"

class Menu;

class WindowBase : public Widget
{
public:
	virtual void Close();

protected:
	WindowBase(int x, int y, int width, int height, const char* title, const Widget* pParent);

	void Create(HWND hParentWnd, int width, int height, const char* title);
	void SetMenuBar(Menu* pMenu);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	Menu* m_pMenu;
};
