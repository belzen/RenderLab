#include "Precompiled.h"
#include "TextBox.h"
#include "UtilsLib/Util.h"

TextBox* TextBox::Create(const Widget& rParent, int x, int y, int width, int height,
	ChangedFunc changedCallback, void* pUserData)
{
	return new TextBox(rParent, x, y, width, height, changedCallback, pUserData);
}

TextBox::TextBox(const Widget& rParent, int x, int y, int width, int height,
	ChangedFunc changedCallback, void* pUserData)
	: Widget(x, y, width, height, &rParent, TextBox::WndProc)
	, m_changedCallback(changedCallback)
	, m_pUserData(pUserData)
{
	// Create child textbox control
	m_hTextBox = CreateWidgetWindow(GetWindowHandle(), "Edit", 0, 0, width, height, 0, WS_EX_CLIENTEDGE);
}

TextBox::~TextBox()
{
	DestroyWindow(m_hTextBox);
}

void TextBox::SetValue(const std::string& val, bool triggerCallback)
{
	SetWindowTextA(m_hTextBox, val.c_str());

	if (triggerCallback && m_changedCallback)
	{
		m_changedCallback(val, m_pUserData);
	}
}

LRESULT CALLBACK TextBox::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		int evt = HIWORD(wParam);
		if (evt == EN_KILLFOCUS)
		{
			char str[256];
			TextBox* pTextBox = (TextBox*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
			GetWindowTextA(pTextBox->m_hTextBox, str, ARRAY_SIZE(str));
			pTextBox->SetValue(str, true);
		}
		break;
	}

	return Widget::WndProc(hWnd, msg, wParam, lParam);
}
