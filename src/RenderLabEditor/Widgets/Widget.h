#pragma once

#include <windows.h>

enum class Units
{
	kPixels,
	kPercentage
};

enum class HAlign
{
	kLeft,
	kCenter,
	kRight
};

enum class VAlign
{
	kTop,
	kCenter,
	kBottom
};

class Widget
{
public:
	void Release();

	int GetX() const;
	int GetY() const;
	int GetWidth() const;
	int GetHeight() const;

	HWND GetWindowHandle() const;

protected:
	static void RegisterWindowClass(const char* className, WNDPROC wndProc);
	static HWND CreateWidgetWindow(HWND hParentWnd, const char* className, int x, int y, int width, int height, DWORD style = 0, DWORD styleEx = 0);

	Widget();
	virtual ~Widget();

	// Create the root/container window for the widget.
	void CreateRootWidgetWindow(HWND hParentWnd, const char* className, int x, int y, int width, int height, DWORD style = 0, DWORD styleEx = 0);

public:
	HWND m_hWidget;

	int m_x;
	int m_y;
	int m_width;
	int m_height;

	Units m_posUnits;
	Units m_sizeUnits;
	HAlign m_horizontalAlign;
	VAlign m_verticalAlign;
};

inline int Widget::GetX() const
{ 
	return m_x; 
}

inline int Widget::GetY() const
{
	return m_x;
}

inline int Widget::GetWidth() const
{
	return m_width;
}

inline int Widget::GetHeight() const
{
	return m_height;
}

inline HWND Widget::GetWindowHandle() const
{
	return m_hWidget;
}
