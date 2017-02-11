#include "Precompiled.h"
#include "Widget.h"
#include "UI.h"

namespace
{
	HFONT s_hDefaultFont = NULL;

	void registerWindowClass(const char* className, WNDPROC wndProc)
	{
		WNDCLASSEXA wc = { 0 };
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
		wc.lpfnWndProc = wndProc;
		wc.hInstance = ::GetModuleHandle(NULL);
		wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
		wc.lpszClassName = className;

		::RegisterClassExA(&wc);
	}
}

void Widget::SetDefaultFont(HFONT hFont)
{
	s_hDefaultFont = hFont;
}

Widget::Widget(int x, int y, int width, int height, const Widget* pParent, DWORD style, WNDPROC pWndProc)
	: m_x(x), m_y(y), m_width(width), m_height(height)
{
	static const char* kDefaultWidgetClassName = "WidgetContainer";
	static bool bRegisteredClass = false;
	if (!bRegisteredClass)
	{
		registerWindowClass(kDefaultWidgetClassName, Widget::WndProc);
		bRegisteredClass = true;
	}

	InitCommon(pParent, kDefaultWidgetClassName, style);

	HWND hParentWnd = pParent ? pParent->GetWindowHandle() : 0;
	if (pWndProc)
	{
		::SetWindowLongPtr(m_hWidget, GWLP_WNDPROC, (LONG_PTR)pWndProc);
	}
}

Widget::Widget(int x, int y, int width, int height, const Widget* pParent, DWORD style, const char* windowClassName)
	: m_x(x), m_y(y), m_width(width), m_height(height)
{
	InitCommon(pParent, windowClassName, style);
}

void Widget::InitCommon(const Widget* pParent, const char* windowClassName, DWORD style)
{
	HWND hParentWnd = pParent ? pParent->GetWindowHandle() : 0;
	m_hWidget = CreateChildWindow(hParentWnd, windowClassName, m_x, m_y, m_width, m_height, style, 0);

	// Setup default drag data.
	m_dragData.type = WidgetDragDataType::kNone;
	m_dragData.pWidget = this;
	m_dragData.data.pTypeless = nullptr;

	m_borderSize = (style & WS_BORDER) ? 1 : 0;
	m_scrollRate = 1;
	m_mouseCaptureCount = 0;
	m_bTrackingMouse = false;
}

Widget::~Widget()
{
	DestroyWindow(m_hWidget);
}

void Widget::Release()
{
	delete this;
}

const WidgetDragData& Widget::GetDragData() const
{
	return m_dragData;
}

void Widget::SetDragData(WidgetDragDataType dataType, void* pData)
{
	m_dragData.type = dataType;
	m_dragData.data.pTypeless = pData;
}

void Widget::SetStyle(DWORD style)
{
	m_windowStyle = style;
	::SetWindowLongPtr(m_hWidget, GWL_STYLE, m_windowStyle);
	::SetWindowPos(m_hWidget, 0, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

void Widget::SetVisible(bool visible)
{
	if (visible)
	{
		SetStyle(m_windowStyle | WS_VISIBLE);
	}
	else
	{
		SetStyle(m_windowStyle & ~WS_VISIBLE);
	}
}

void Widget::SetInputEnabled(bool enabled)
{
	if (enabled)
	{
		SetStyle(m_windowStyle & ~WS_DISABLED);
	}
	else
	{
		SetStyle(m_windowStyle | WS_DISABLED);
	}
}

void Widget::SetBorder(bool enabled)
{
	if (enabled)
	{
		SetStyle(m_windowStyle | WS_BORDER);
		m_borderSize = 1;
	}
	else
	{
		SetStyle(m_windowStyle & ~WS_BORDER);
		m_borderSize = 0;
	}
}

void Widget::SetScroll(int rangeMax, int scrollRate, int pageSize)
{
	m_scrollRate = scrollRate;

	SCROLLINFO scrollInfo;
	scrollInfo.cbSize = sizeof(scrollInfo);
	scrollInfo.fMask = SIF_RANGE | SIF_PAGE | SIF_DISABLENOSCROLL;
	scrollInfo.nMin = 0;
	scrollInfo.nMax = rangeMax;
	scrollInfo.nPage = pageSize;

	::SetScrollInfo(m_hWidget, SB_VERT, &scrollInfo, true);
}

void Widget::SetKeyDownCallback(KeyDownFunc callback, void* pUserData)
{
	m_keyDownCallback = callback;
	m_pKeyDownUserData = pUserData;
}

void Widget::SetKeyUpCallback(KeyUpFunc callback, void* pUserData)
{
	m_keyUpCallback = callback;
	m_pKeyUpUserData = pUserData;
}

void Widget::SetMouseClickedCallback(MouseClickedFunc callback, void* pUserData)
{
	m_mouseClickedCallback = callback;
	m_pMouseClickedUserData = pUserData;
}

void Widget::SetMouseDoubleClickedCallback(MouseDoubleClickedFunc callback, void* pUserData)
{
	m_mouseDoubleClickedCallback = callback;
	m_pMouseDoubleClickedUserData = pUserData;
}

void Widget::SetMouseDownCallback(MouseDownFunc callback, void* pUserData)
{
	m_mouseDownCallback = callback;
	m_pMouseDownUserData = pUserData;
}

void Widget::SetMouseUpCallback(MouseUpFunc callback, void* pUserData)
{
	m_mouseUpCallback = callback;
	m_pMouseUpUserData = pUserData;
}

void Widget::SetMouseEnterCallback(MouseEnterFunc callback, void* pUserData)
{
	m_mouseEnterCallback = callback;
	m_pMouseEnterUserData = pUserData;
}

void Widget::SetMouseLeaveCallback(MouseLeaveFunc callback, void* pUserData)
{
	m_mouseLeaveCallback = callback;
	m_pMouseLeaveUserData = pUserData;
}

HWND Widget::CreateChildWindow(const HWND hParentWnd, const char* className, int x, int y, int width, int height, DWORD style, DWORD styleEx)
{
	DWORD windowType = hParentWnd ? WS_CHILD : WS_OVERLAPPEDWINDOW;
	RECT wr = { 0, 0, width, height };
	::AdjustWindowRect(&wr, windowType, false);

	m_windowStyle = windowType | style | WS_VISIBLE;

	HWND hWnd = ::CreateWindowExA(
		styleEx,
		className,
		0,
		m_windowStyle,
		x, y, (wr.right - wr.left), (wr.bottom - wr.top),
		hParentWnd,
		NULL,
		::GetModuleHandle(NULL),
		NULL);

	::SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)this);

	if (s_hDefaultFont)
	{
		::SendMessageA(hWnd, WM_SETFONT, (WPARAM)s_hDefaultFont, true);
	}
	return hWnd;
}

void Widget::SetPosition(int x, int y)
{
	m_x = x;
	m_y = y;
	::MoveWindow(m_hWidget, m_x, m_y, m_width, m_height, true);
}

void Widget::SetSize(int w, int h)
{
	m_width = w;
	m_height = h;
	::MoveWindow(m_hWidget, m_x, m_y, m_width, m_height, true);
}

bool Widget::HandleResize(int newWidth, int newHeight)
{ 
	return OnResize(newWidth, newHeight); 
}

bool Widget::HandleKeyDown(int key)
{ 
	if (m_keyDownCallback)
	{
		m_keyDownCallback(this, key, m_pKeyDownUserData);
	}

	return OnKeyDown(key);
}

bool Widget::HandleKeyUp(int key)
{
	if (m_keyUpCallback)
	{
		m_keyUpCallback(this, key, m_pKeyUpUserData);
	}

	return OnKeyUp(key);
}

bool Widget::HandleChar(char c)
{
	return OnChar(c);
}

bool Widget::HandleMouseWheel(int delta)
{
	bool bHandled = false;
	if (m_windowStyle & WS_VSCROLL)
	{
		::SendMessage(m_hWidget, WM_VSCROLL, ((delta < 0) ? SB_PAGEDOWN : SB_PAGEUP), NULL);
		bHandled = true;
	}

	return OnMouseWheel(delta) || bHandled;
}

bool Widget::HandleMouseDown(int button, int mx, int my)
{
	::SetFocus(m_hWidget);

	if (ShouldCaptureMouse())
	{
		CaptureMouse();
	}

	if (button == 0 && m_dragData.type != WidgetDragDataType::kNone)
	{
		UI::SetDraggedWidget(this);
	}

	if (m_mouseDownCallback)
	{
		m_mouseDownCallback(this, 0, m_pMouseDownUserData);
	}

	return OnMouseDown(button, mx, my);
}

bool Widget::HandleMouseUp(int button, int mx, int my)
{
	if (ShouldCaptureMouse())
	{
		ReleaseMouse();
	}

	if (button == 0 && UI::GetDraggedWidget())
	{
		POINT pt;
		GetCursorPos(&pt);

		HWND hTargetWnd = WindowFromPoint(pt);
		Widget* pTargetWidget = (Widget*)GetWindowLongPtr(hTargetWnd, GWLP_USERDATA);
		if (pTargetWidget)
		{
			pTargetWidget->HandleDragEnd(UI::GetDraggedWidget());
		}

		UI::SetDraggedWidget(nullptr);
	}

	bool bHandled = false;
	if (m_mouseCaptureCount > 0)
	{
		bHandled = HandleMouseClick(button);
	}

	if (m_mouseUpCallback)
	{
		m_mouseUpCallback(this, 0, m_pMouseUpUserData);
	}

	return OnMouseUp(button, mx, my);
}

bool Widget::HandleMouseMove(int mx, int my)
{
	if (!m_bTrackingMouse)
	{
		TRACKMOUSEEVENT track;
		track.cbSize = sizeof(TRACKMOUSEEVENT);
		track.hwndTrack = m_hWidget;
		track.dwFlags = TME_LEAVE;
		TrackMouseEvent(&track);
	
		m_bTrackingMouse = true;

		HandleMouseEnter(mx, my);
	}

	return OnMouseMove(mx, my);
}

bool Widget::HandleMouseEnter(int mx, int my)
{
	return OnMouseEnter(mx, my);
}

bool Widget::HandleMouseLeave()
{
	m_bTrackingMouse = false;
	return OnMouseLeave();
}

bool Widget::HandleMouseClick(int button)
{
	if (m_mouseClickedCallback)
	{
		m_mouseClickedCallback(this, button, m_pMouseClickedUserData);
	}

	return OnMouseClick(button);
}

bool Widget::HandleMouseDoubleClick(int button)
{
	bool bHandled = OnMouseDoubleClick(button);

	if (m_mouseDoubleClickedCallback)
	{
		m_mouseDoubleClickedCallback(this, button, m_pMouseDoubleClickedUserData);
	}

	return bHandled;
}

void Widget::HandleDragEnd(Widget* pDraggedWidget)
{
	OnDragEnd(pDraggedWidget);
}

bool Widget::HandleClose() 
{ 
	return OnClose();
}

void Widget::CaptureMouse()
{
	if (m_mouseCaptureCount == 0)
		::SetCapture(m_hWidget);

	++m_mouseCaptureCount;
}

void Widget::ReleaseMouse()
{
	if (m_mouseCaptureCount > 0)
	{
		--m_mouseCaptureCount;
		if (m_mouseCaptureCount == 0)
			::ReleaseCapture();
	}
}

LRESULT CALLBACK Widget::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Widget* pWidget = (Widget*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (pWidget)
	{
		bool bHandled = false;

		switch (msg)
		{
		case WM_MOUSEMOVE:
			bHandled = pWidget->HandleMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_MOUSELEAVE:
			bHandled = pWidget->HandleMouseLeave();
			break;
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			{
				int button = (msg == WM_LBUTTONDOWN) ? 0 : 1;
				bHandled = pWidget->HandleMouseDown(button, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			}
			break;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			{
				int button = (msg == WM_LBUTTONUP) ? 0 : 1;
				bHandled = pWidget->HandleMouseUp(button, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			}
			break;
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
			{
				int button = (msg == WM_LBUTTONDBLCLK) ? 0 : 1;
				bHandled = pWidget->HandleMouseDoubleClick(button);
			}
			break;
		case WM_MOUSEWHEEL:
			bHandled = pWidget->HandleMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
			break;
		case WM_CHAR:
			bHandled = pWidget->HandleChar((char)wParam);
			break;
		case WM_KEYDOWN:
			bHandled = pWidget->HandleKeyDown((int)wParam);
			break;
		case WM_KEYUP:
			bHandled = pWidget->HandleKeyUp((int)wParam);
			break;
		case WM_SIZE:
			pWidget->m_width = LOWORD(lParam) + (pWidget->m_borderSize * 2);
			if (pWidget->m_windowStyle & WS_VSCROLL)
			{
				pWidget->m_width += GetSystemMetrics(SM_CXVSCROLL);
			}
			pWidget->m_height = HIWORD(lParam) + (pWidget->m_borderSize * 2);
			bHandled = pWidget->HandleResize(pWidget->GetWidth(), pWidget->GetHeight());
			break;
		case WM_CLOSE:
			bHandled = pWidget->HandleClose();
			break;
		case WM_CANCELMODE:
			// Cancel modal state including dragging and mouse capture
			UI::SetDraggedWidget(nullptr);
			pWidget->ReleaseMouse();
			break;
		case WM_CAPTURECHANGED:
			// Mouse capture has been canceled or changed externally.
			UI::SetDraggedWidget(nullptr);
			break;

		case WM_VSCROLL:
			{
				SCROLLINFO scrollInfo;
				scrollInfo.cbSize = sizeof(scrollInfo);
				scrollInfo.fMask = SIF_ALL;
				::GetScrollInfo(hWnd, SB_VERT, &scrollInfo);

				int prevScrollPos = scrollInfo.nPos;
				switch (LOWORD(wParam))
				{
				case SB_TOP:
					scrollInfo.nPos = scrollInfo.nMin;
					break;
				case SB_BOTTOM:
					scrollInfo.nPos = scrollInfo.nMax;
					break;
				case SB_LINEUP:
					scrollInfo.nPos -= 1;
					break;
				case SB_LINEDOWN:
					scrollInfo.nPos += 1;
					break;
				case SB_PAGEUP:
					scrollInfo.nPos -= scrollInfo.nPage;
					break;
				case SB_PAGEDOWN:
					scrollInfo.nPos += scrollInfo.nPage;
					break;
				case SB_THUMBTRACK:
					scrollInfo.nPos = scrollInfo.nTrackPos;
					break;
				}

				// Set and immediately get scroll position.
				// Supposedly, Windows may alter the values (perhaps clamping the value to min/max?)
				scrollInfo.fMask = SIF_POS;
				::SetScrollInfo(hWnd, SB_VERT, &scrollInfo, true);
				::GetScrollInfo(hWnd, SB_VERT, &scrollInfo);

				// Scroll to the new position
				if (scrollInfo.nPos != prevScrollPos)
				{
					::ScrollWindow(hWnd, 0, pWidget->m_scrollRate * (prevScrollPos - scrollInfo.nPos), NULL, NULL);
					::UpdateWindow(hWnd);
				}

				bHandled = true;
			}
			break;
		}

		if (bHandled)
			return 0;
	}

	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}