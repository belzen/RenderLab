#pragma once

#include "WidgetDragData.h"
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

struct NameValuePair
{
	const wchar_t* name;
	int value;
};

class Widget
{
public:
	static void SetDefaultFont(HFONT hFont);

	void Release();

	int GetX() const;
	int GetY() const;
	void SetPosition(int x, int y);

	int GetWidth() const;
	int GetHeight() const;
	void SetSize(int w, int h);

	HWND GetWindowHandle() const;

	void SetDragData(WidgetDragDataType dataType, void* pData);
	const WidgetDragData& GetDragData() const;

	// Show/hide the widget.
	void SetVisible(bool visible);

	// Show/hide border
	void SetBorder(bool enabled);

	// Enable/disable user input on the widget.
	void SetInputEnabled(bool enabled);

	void SetScroll(int rangeMax, int scrollRate, int pageSize);

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

	typedef void(*MouseEnterFunc)(Widget* pWidget, void* pUserData);
	void SetMouseEnterCallback(MouseEnterFunc callback, void* pUserData);

	typedef void(*MouseLeaveFunc)(Widget* pWidget, void* pUserData);
	void SetMouseLeaveCallback(MouseLeaveFunc callback, void* pUserData);

protected:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	Widget(int x, int y, int width, int height, const Widget* pParent, DWORD style, WNDPROC pWndProc = Widget::WndProc);
	Widget(int x, int y, int width, int height, const Widget* pParent, DWORD style, const char* windowClassName);
	virtual ~Widget();

	HWND CreateChildWindow(HWND hParentWnd, const char* className, int x, int y, int width, int height, DWORD style = 0, DWORD styleEx = 0);
	void SetStyle(DWORD style);

	//////////////////////////////////////////////////////////////////////////
	// Input event handling interface
	virtual bool ShouldCaptureMouse() const					{ return false; }
	virtual bool OnResize(int newWidth, int newHeight)		{ return false; }
	virtual bool OnKeyDown(int key)							{ return false; }
	virtual bool OnKeyUp(int key)							{ return false; }
	virtual bool OnChar(char c)								{ return false; }
	virtual bool OnMouseWheel(int delta)					{ return false; }
	virtual bool OnMouseDown(int button, int mx, int my)	{ return false; }
	virtual bool OnMouseUp(int button, int mx, int my)		{ return false; }
	virtual bool OnMouseMove(int mx, int my)				{ return false; }
	virtual bool OnMouseEnter(int mx, int my)				{ return false; }
	virtual bool OnMouseLeave()								{ return false; }
	virtual bool OnMouseClick(int button)					{ return false; }
	virtual bool OnMouseDoubleClick(int button)				{ return false; }
	virtual void OnDragEnd(Widget* pDraggedWidget)			{}
	virtual bool OnClose()									{ return false; }

private:
	void InitCommon(const Widget* pParent, const char* windowClassName, DWORD style);
	void CaptureMouse();
	void ReleaseMouse();

	//////////////////////////////////////////////////////////////////////////
	// Internal handling of input events.
	// Passes through to the On* version of these events as appropriate.
	bool HandleResize(int newWidth, int newHeight);
	bool HandleKeyDown(int key);
	bool HandleKeyUp(int key);
	bool HandleChar(char c);
	bool HandleMouseWheel(int delta);
	bool HandleMouseDown(int button, int mx, int my);
	bool HandleMouseUp(int button, int mx, int my);
	bool HandleMouseMove(int mx, int my);
	bool HandleMouseEnter(int mx, int my);
	bool HandleMouseLeave();
	bool HandleMouseClick(int button);
	bool HandleMouseDoubleClick(int button);
	void HandleDragEnd(Widget* pDraggedWidget);
	bool HandleClose();

private:
	WidgetDragData m_dragData;
	HWND m_hWidget;
	DWORD m_windowStyle;

	int m_x;
	int m_y;
	int m_width;
	int m_height;
	int m_borderSize;

	Units m_posUnits;
	Units m_sizeUnits;
	HAlign m_horizontalAlign;
	VAlign m_verticalAlign;

	int m_scrollRate;

	int m_mouseCaptureCount;
	bool m_bTrackingMouse;

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
	MouseEnterFunc m_mouseEnterCallback;
	void* m_pMouseEnterUserData;
	MouseLeaveFunc m_mouseLeaveCallback;
	void* m_pMouseLeaveUserData;
};

inline int Widget::GetX() const
{ 
	return m_x; 
}

inline int Widget::GetY() const
{
	return m_y;
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
