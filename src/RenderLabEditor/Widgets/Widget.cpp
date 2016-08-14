#include "Precompiled.h"
#include "Widget.h"


Widget::Widget()
	: m_x(0), m_y(0), m_width(0), m_height(0)
{

}

Widget::~Widget()
{
	DestroyWindow(m_hWidget);
}

void Widget::Release()
{
	delete this;
}

void Widget::RegisterWindowClass(const char* className, WNDPROC wndProc)
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

void Widget::CreateRootWidgetWindow(HWND hParentWnd, const char* className, int x, int y, int width, int height, DWORD style, DWORD styleEx)
{
	m_x = x;
	m_y = y;
	m_width = width;
	m_height = height;

	m_hWidget = CreateWidgetWindow(hParentWnd, className, x, y, width, height, style, styleEx);
	::SetWindowLongPtr(m_hWidget, GWLP_USERDATA, (LONG_PTR)this);
}