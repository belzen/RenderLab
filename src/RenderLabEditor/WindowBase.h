#pragma once

#include <windows.h>
#include "input\Input.h"
#include "Widgets/Widget.h"

class Menu;

class WindowBase : public Widget
{
public:
	void Close();

	void Resize(int parentWidth, int parentHeight);

	int GetWidth() const { return m_width; }
	int GetHeight() const { return m_height; }

protected:
	void Create(HWND hParentWnd, int width, int height, const char* title);
	void SetMenuBar(Menu* pMenu);

	virtual bool HandleResize(int newWidth, int newHeight) { return false; }
	virtual bool HandleKeyDown(int key) { return false; }
	virtual bool HandleKeyUp(int key) { return false; }
	virtual bool HandleMouseDown(int button, int mx, int my) { return false; }
	virtual bool HandleMouseUp(int button, int mx, int my) { return false; }
	virtual bool HandleMouseMove(int mx, int my) { return false; }
	virtual bool HandleClose() { return false; }

private:
	static LRESULT CALLBACK WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

private:
	Menu* m_pMenu;

	int m_mouseCaptureCount;
	int m_width;
	int m_height;
};
