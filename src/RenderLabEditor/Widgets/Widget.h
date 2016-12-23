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
	typedef void(*ClickedFunc)(Widget* pWidget, void* pUserData);


	void Release();

	int GetX() const;
	int GetY() const;
	void SetPosition(int x, int y);

	int GetWidth() const;
	int GetHeight() const;
	void SetSize(int w, int h);

	HWND GetWindowHandle() const;

protected:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static HWND CreateWidgetWindow(HWND hParentWnd, const char* className, int x, int y, int width, int height, DWORD style = 0, DWORD styleEx = 0);

	Widget(int x, int y, int width, int height, const Widget* pParent, WNDPROC pWndProc = Widget::WndProc);
	Widget(int x, int y, int width, int height, const Widget* pParent, const char* windowClassName);
	virtual ~Widget();

	virtual bool HandleResize(int newWidth, int newHeight) { return false; }
	virtual bool HandleKeyDown(int key) { return false; }
	virtual bool HandleKeyUp(int key) { return false; }
	virtual bool HandleMouseDown(int button, int mx, int my) { return false; }
	virtual bool HandleMouseUp(int button, int mx, int my) { return false; }
	virtual bool HandleMouseMove(int mx, int my) { return false; }
	virtual bool HandleClose() { return false; }

private:
	void InitCommon(const Widget* pParent, const char* windowClassName);

	HWND m_hWidget;

	int m_x;
	int m_y;
	int m_width;
	int m_height;

	Units m_posUnits;
	Units m_sizeUnits;
	HAlign m_horizontalAlign;
	VAlign m_verticalAlign;

	int m_mouseCaptureCount;

	ClickedFunc m_clickedCallback;
	void* m_pClickedUserData;
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
