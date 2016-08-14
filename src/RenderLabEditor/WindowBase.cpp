#include "Precompiled.h"
#include "WindowBase.h"
#include "Widgets/Menu.h"

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
	const char* kWndClassName = "EditorWindow";
	static bool s_bRegisteredClass = false;
	if (!s_bRegisteredClass)
	{
		RegisterWindowClass(kWndClassName, WindowBase::WinProc);
		s_bRegisteredClass = true;
	}

	CreateRootWidgetWindow(hParentWnd, kWndClassName, 0, 0, width, height);
	::ShowWindow(GetWindowHandle(), 1);
}

void WindowBase::SetMenuBar(Menu* pMenu)
{
	m_pMenu = pMenu;
	::SetMenu(GetWindowHandle(), pMenu->GetMenuHandle());
	::DrawMenuBar(GetWindowHandle());
}

LRESULT CALLBACK WindowBase::WinProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowBase* pWindow = (WindowBase*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
	::MoveWindow(GetWindowHandle(), 0, 0, parentWidth, parentHeight, true);
}