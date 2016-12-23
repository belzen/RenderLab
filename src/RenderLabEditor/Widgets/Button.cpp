#include "Precompiled.h"
#include "Button.h"

Button* Button::Create(const Widget& rParent, int x, int y, int width, int height,
	const char* text, ClickedFunc clickedCallback, void* pUserData)
{
	return new Button(rParent, x, y, width, height, text, clickedCallback, pUserData);
}

Button::Button(const Widget& rParent, int x, int y, int width, int height, 
	const char* text, ClickedFunc clickedCallback, void* pUserData)
	: Widget(x, y, width, height, &rParent, Button::WndProc)
{
	// Create check box control
	m_hButton = CreateWidgetWindow(GetWindowHandle(), "Button", 0, 0, width, height);
	::SetWindowTextA(m_hButton, text);

	m_clickedCallback = clickedCallback;
	m_pUserData = pUserData;
}

Button::~Button()
{
	DestroyWindow(m_hButton);
}

LRESULT CALLBACK Button::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		{
			Button* pButton = (Button*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
			if (pButton->m_clickedCallback)
			{
				pButton->m_clickedCallback(pButton->m_pUserData);
			}
		}
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}