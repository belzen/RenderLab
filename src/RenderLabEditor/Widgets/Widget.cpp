#include "Precompiled.h"
#include "Widget.h"

namespace
{
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

Widget::Widget(int x, int y, int width, int height, const Widget* pParent, WNDPROC pWndProc)
	: m_x(x), m_y(y), m_width(width), m_height(height)
{
	static const char* kDefaultWidgetClassName = "WidgetContainer";
	static bool bRegisteredClass = false;
	if (!bRegisteredClass)
	{
		registerWindowClass(kDefaultWidgetClassName, Widget::WndProc);
		bRegisteredClass = true;
	}

	InitCommon(pParent, kDefaultWidgetClassName);

	HWND hParentWnd = pParent ? pParent->GetWindowHandle() : 0;
	if (pWndProc)
	{
		::SetWindowLongPtr(m_hWidget, GWLP_WNDPROC, (LONG_PTR)pWndProc);
	}
}

Widget::Widget(int x, int y, int width, int height, const Widget* pParent, const char* windowClassName)
	: m_x(x), m_y(y), m_width(width), m_height(height)
{
	InitCommon(pParent, windowClassName);
}

void Widget::InitCommon(const Widget* pParent, const char* windowClassName)
{
	HWND hParentWnd = pParent ? pParent->GetWindowHandle() : 0;
	m_hWidget = CreateWidgetWindow(hParentWnd, windowClassName, m_x, m_y, m_width, m_height, 0, 0);
	::SetWindowLongPtr(m_hWidget, GWLP_USERDATA, (LONG_PTR)this);
}

Widget::~Widget()
{
	DestroyWindow(m_hWidget);
}

void Widget::Release()
{
	delete this;
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

void Widget::SetMouseOverCallback(MouseOverFunc callback, void* pUserData)
{
	m_mouseOverCallback = callback;
	m_pMouseOverUserData = pUserData;
}

void Widget::SetMouseOutCallback(MouseOutFunc callback, void* pUserData)
{
	m_mouseOutCallback = callback;
	m_pMouseOutUserData = pUserData;
}

HWND Widget::CreateWidgetWindow(const HWND hParentWnd, const char* className, int x, int y, int width, int height, DWORD style, DWORD styleEx)
{
	DWORD windowType = hParentWnd ? WS_CHILD : WS_OVERLAPPEDWINDOW;
	RECT wr = { 0, 0, width, height };
	::AdjustWindowRect(&wr, windowType, false);

	return ::CreateWindowExA(
		styleEx,
		className,
		0,
		windowType | style | WS_VISIBLE,
		x, y, (wr.right - wr.left), (wr.bottom - wr.top),
		hParentWnd,
		NULL,
		::GetModuleHandle(NULL),
		NULL);
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
	return false; 
}

bool Widget::HandleKeyDown(int key)
{ 
	if (m_keyDownCallback)
	{
		m_keyDownCallback(this, key, m_pKeyDownUserData);
	}
	return false; 
}

bool Widget::HandleKeyUp(int key)
{
	if (m_keyUpCallback)
	{
		m_keyUpCallback(this, key, m_pKeyUpUserData);
	}

	return false;
}

bool Widget::HandleMouseDown(int button, int mx, int my)
{
	if (m_mouseDownCallback)
	{
		m_mouseDownCallback(this, 0, m_pMouseDownUserData);
	}

	return false;
}

bool Widget::HandleMouseUp(int button, int mx, int my)
{
	bool bHandled = false;

	if (m_mouseUpCallback)
	{
		m_mouseUpCallback(this, 0, m_pMouseUpUserData);
	}

	if (m_mouseCaptureCount > 0)
	{
		bHandled = HandleMouseClick(button);
	}

	return bHandled;
}

bool Widget::HandleMouseMove(int mx, int my)
{
	return false;
}

bool Widget::HandleMouseClick(int button)
{
	if (m_mouseClickedCallback)
	{
		m_mouseClickedCallback(this, button, m_pMouseClickedUserData);
	}

	return false;
}

bool Widget::HandleMouseDoubleClick(int button)
{
	if (m_mouseDoubleClickedCallback)
	{
		m_mouseDoubleClickedCallback(this, button, m_pMouseDoubleClickedUserData);
	}

	return false;
}

bool Widget::HandleClose() 
{ 
	return false; 
}

void Widget::CaptureMouse(HWND hWnd)
{
	if (m_mouseCaptureCount == 0)
		::SetCapture(hWnd);

	++m_mouseCaptureCount;
}

void Widget::ReleaseMouse()
{
	--m_mouseCaptureCount;
	if (m_mouseCaptureCount == 0)
		::ReleaseCapture();
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
		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			{
				int button = (msg == WM_LBUTTONDOWN) ? 0 : 1;
				pWidget->CaptureMouse(hWnd);
				::SetFocus(hWnd);
				bHandled = pWidget->HandleMouseDown(button, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			}
			break;
		case WM_LBUTTONUP:
		case WM_RBUTTONUP:
			{
				int button = (msg == WM_LBUTTONUP) ? 0 : 1;
				bHandled = pWidget->HandleMouseUp(button, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
				pWidget->ReleaseMouse();
			}
			break;
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDBLCLK:
			{
				int button = (msg == WM_LBUTTONDBLCLK) ? 0 : 1;
				bHandled = pWidget->HandleMouseDoubleClick(button);
			}
			break;
		case WM_KEYDOWN:
			bHandled = pWidget->HandleKeyDown((int)wParam);
			break;
		case WM_KEYUP:
			bHandled = pWidget->HandleKeyUp((int)wParam);
			break;
		case WM_SIZE:
			pWidget->m_width = LOWORD(lParam);
			pWidget->m_height = HIWORD(lParam);;
			bHandled = pWidget->HandleResize(pWidget->GetWidth(), pWidget->GetHeight());
			break;
		}

		if (bHandled)
			return 0;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}