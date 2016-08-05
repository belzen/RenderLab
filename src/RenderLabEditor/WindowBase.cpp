#include "WindowBase.h"
#include "Menu.h"
#include <windowsx.h>

namespace
{
	void captureMouse(HWND hWnd, int& rMouseCaptureCount)
	{
		if (rMouseCaptureCount == 0)
			::SetCapture(hWnd);

		++rMouseCaptureCount;
	}

	void releaseMouse(int& rMouseCaptureCount)
	{
		--rMouseCaptureCount;
		if (rMouseCaptureCount == 0)
			::ReleaseCapture();
	}
}

void WindowBase::Create(HWND hParentWnd, int width, int height, const char* title)
{
	DWORD dwStyle = hParentWnd ? WS_CHILD : WS_OVERLAPPEDWINDOW;

	HINSTANCE hInstance = ::GetModuleHandle(0);
	WNDCLASSEX wc = { 0 };
	RECT wr = { 0, 0, width, height };
	::AdjustWindowRect(&wr, dwStyle, false);

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowBase::WinProc;
	wc.hInstance = hInstance;
	wc.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = L"EditorWindow";

	::RegisterClassEx(&wc);

	WCHAR wtitle[128] = { 0 };
	mbstowcs_s(nullptr, wtitle, title, strlen(title));
	m_hWnd = ::CreateWindowEx(NULL,
		wc.lpszClassName, wtitle,
		dwStyle,
		0, 0,
		wr.right - wr.left, wr.bottom - wr.top,
		hParentWnd, NULL, hInstance, NULL);

	::SetWindowLongPtr(m_hWnd, GWLP_USERDATA, (LONG_PTR)this);
	::ShowWindow(m_hWnd, 1);
}

void WindowBase::SetMenuBar(Menu* pMenu)
{
	m_pMenu = pMenu;
	::SetMenu(m_hWnd, pMenu->GetMenuHandle());
	::DrawMenuBar(m_hWnd);
}

LRESULT CALLBACK WindowBase::WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowBase* pWindow = (WindowBase*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (pWindow)
	{
		bool bHandled = false;
		switch (message)
		{
		case WM_MOUSEMOVE:
			bHandled = pWindow->HandleMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_LBUTTONDOWN:
			captureMouse(hWnd, pWindow->m_mouseCaptureCount);
			::SetFocus(hWnd);
			bHandled = pWindow->HandleMouseDown(0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_LBUTTONUP:
			bHandled = pWindow->HandleMouseUp(0, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			releaseMouse(pWindow->m_mouseCaptureCount);
			break;
		case WM_RBUTTONDOWN:
			captureMouse(hWnd, pWindow->m_mouseCaptureCount);
			::SetFocus(hWnd);
			bHandled = pWindow->HandleMouseDown(1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			break;
		case WM_RBUTTONUP:
			bHandled = pWindow->HandleMouseUp(1, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
			releaseMouse(pWindow->m_mouseCaptureCount);
			break;
		case WM_KEYDOWN:
			bHandled = pWindow->HandleKeyDown((int)wParam);
			break;
		case WM_KEYUP:
			bHandled = pWindow->HandleKeyUp((int)wParam);
			break;
		case WM_SIZE:
			pWindow->m_width = LOWORD(lParam);
			pWindow->m_height = HIWORD(lParam);
			bHandled = pWindow->HandleResize(pWindow->m_width, pWindow->m_height);
			break;
		case WM_COMMAND:
			if (pWindow->m_pMenu)
			{
				bHandled = pWindow->m_pMenu->HandleMenuCommand((int)wParam);
			}
			break;
		case WM_CLOSE:
			bHandled = pWindow->HandleClose();
			break;
		}

		if (bHandled)
			return 0;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

void WindowBase::Close()
{
}

void WindowBase::Resize(int parentWidth, int parentHeight)
{
	::MoveWindow(m_hWnd, 0, 0, parentWidth, parentHeight, true);
}