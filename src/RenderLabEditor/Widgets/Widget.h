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
	void SetPosition(int x, int y);

	int GetWidth() const;
	int GetHeight() const;
	void SetSize(int w, int h);

	HWND GetWindowHandle() const;

	//////////////////////////////////////////////////////////////////////////
	// Input callbacks
	typedef void(*KeyDownFunc)(Widget* pWidget, int key, void* pUserData);
	void SetKeyDownCallback(KeyDownFunc callback, void* pUserData);

	typedef void(*KeyUpFunc)(Widget* pWidget, int key, void* pUserData);
	void SetKeyUpCallback(KeyUpFunc callback, void* pUserData);

	typedef void(*MouseClickedFunc)(Widget* pWidget, int button, void* pUserData);
	void SetMouseClickedCallback(MouseClickedFunc callback, void* pUserData);

	typedef void(*MouseDoubleClickedFunc)(Widget* pWidget, int button, void* pUserData);
	void SetMouseDoubleClickedCallback(MouseDoubleClickedFunc callback, void* pUserData);

	typedef void(*MouseDownFunc)(Widget* pWidget, int button, void* pUserData);
	void SetMouseDownCallback(MouseDownFunc callback, void* pUserData);

	typedef void(*MouseUpFunc)(Widget* pWidget, int button, void* pUserData);
	void SetMouseUpCallback(MouseUpFunc callback, void* pUserData);

	typedef void(*MouseOverFunc)(Widget* pWidget, void* pUserData);
	void SetMouseOverCallback(MouseOverFunc callback, void* pUserData);

	typedef void(*MouseOutFunc)(Widget* pWidget, void* pUserData);
	void SetMouseOutCallback(MouseOutFunc callback, void* pUserData);

protected:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static HWND CreateWidgetWindow(HWND hParentWnd, const char* className, int x, int y, int width, int height, DWORD style = 0, DWORD styleEx = 0);

	Widget(int x, int y, int width, int height, const Widget* pParent, WNDPROC pWndProc = Widget::WndProc);
	Widget(int x, int y, int width, int height, const Widget* pParent, const char* windowClassName);
	virtual ~Widget();

	virtual bool HandleResize(int newWidth, int newHeight);
	virtual bool HandleKeyDown(int key);
	virtual bool HandleKeyUp(int key);
	virtual bool HandleMouseDown(int button, int mx, int my);
	virtual bool HandleMouseUp(int button, int mx, int my);
	virtual bool HandleMouseMove(int mx, int my);
	virtual bool HandleMouseClick(int button);
	virtual bool HandleMouseDoubleClick(int button);
	virtual bool HandleClose();

private:
	void InitCommon(const Widget* pParent, const char* windowClassName);
	void CaptureMouse(HWND hWnd);
	void ReleaseMouse();

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

	KeyDownFunc m_keyDownCallback;
	void* m_pKeyDownUserData;
	KeyUpFunc m_keyUpCallback;
	void* m_pKeyUpUserData;
	MouseClickedFunc m_mouseClickedCallback;
	void* m_pMouseClickedUserData;
	MouseDoubleClickedFunc m_mouseDoubleClickedCallback;
	void* m_pMouseDoubleClickedUserData;
	MouseDownFunc m_mouseDownCallback;
	void* m_pMouseDownUserData;
	MouseUpFunc m_mouseUpCallback;
	void* m_pMouseUpUserData;
	MouseOverFunc m_mouseOverCallback;
	void* m_pMouseOverUserData;
	MouseOutFunc m_mouseOutCallback;
	void* m_pMouseOutUserData;
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
