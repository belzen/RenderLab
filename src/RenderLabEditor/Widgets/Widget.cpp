#include "Precompiled.h"
#include "Widget.h"

namespace
{
	void registerWindowClass(const char* className, WNDPROC wndProc)
	{
		WNDCLASSEXA wc = { 0 };
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = wndProc;
		wc.hInstance = ::GetModuleHandle(NULL);
		wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
		wc.lpszClassName = className;

		::RegisterClassExA(&wc);
	}

	void captureMouse(HWND hWnd, int* pMouseCaptureCount)
	{
		if (*pMouseCaptureCount == 0)
			::SetCapture(hWnd);

		++(*pMouseCaptureCount);
	}

	void releaseMouse(int* pMouseCaptureCount)
	{
		--(*pMouseCaptureCount);
		if ((*pMouseCaptureCount) == 0)
			::ReleaseCapture();
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
			captureMouse(hWnd, &pWidget->m_mouseCaptureCount);
			::SetFocus(hWnd);
			bHandled = pWidget->HandleMouseDown(0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));

			if (pWidget->m_clickedCallback)
			{
				pWidget->m_clickedCallback(pWidget, pWidget->m_pClickedUserData);
			}
			break;
		case WM_LBUTTONUP:
			bHandled = pWidget->HandleMouseUp(0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			releaseMouse(&pWidget->m_mouseCaptureCount);
			break;
		case WM_RBUTTONDOWN:
			captureMouse(hWnd, &pWidget->m_mouseCaptureCount);
			::SetFocus(hWnd);
			bHandled = pWidget->HandleMouseDown(1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_RBUTTONUP:
			bHandled = pWidget->HandleMouseUp(1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			releaseMouse(&pWidget->m_mouseCaptureCount);
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