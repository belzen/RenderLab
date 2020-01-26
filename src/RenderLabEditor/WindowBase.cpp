#include "Precompiled.h"
#include "WindowBase.h"
#include "Widgets/Menu.h"


WindowBase::WindowBase(int x, int y, int width, int height, const char* title, const Widget* pParent, WNDPROC pWndProc)
	: Widget(x, y, width, height, pParent, 0, pWndProc ? pWndProc : WindowBase::WndProc)
{
	::SetWindowTextA(GetWindowHandle(), title);
	::ShowWindow(GetWindowHandle(), 1);
}

void WindowBase::SetMenuBar(Menu* pMenu)
{
	m_pMenu = pMenu;
	::SetMenu(GetWindowHandle(), pMenu->GetMenuHandle());
	::DrawMenuBar(GetWindowHandle());
}

LRESULT CALLBACK WindowBase::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	WindowBase* pWindow = (WindowBase*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
	if (pWindow)
	{
		bool bHandled = false;
		switch (message)
		{
		case WM_COMMAND:
			if (pWindow->m_pMenu)
			{
				bHandled = pWindow->m_pMenu->HandleMenuCommand((int)wParam);
			}
			break;
		}

		if (bHandled)
			return 0;
	}

	return Widget::WndProc(hWnd, message, wParam, lParam);
}

void WindowBase::Close()
{
}
