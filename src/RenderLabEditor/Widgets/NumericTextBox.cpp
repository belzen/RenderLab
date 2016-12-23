#include "Precompiled.h"
#include "NumericTextBox.h"
#include "UtilsLib\Util.h"

NumericTextBox* NumericTextBox::Create(const Widget& rParent, int x, int y, int width, int height,
	ChangedFunc changedCallback, void* pUserData)
{
	return new NumericTextBox(rParent, x, y, width, height, changedCallback, pUserData);
}

NumericTextBox::NumericTextBox(const Widget& rParent, int x, int y, int width, int height,
	ChangedFunc changedCallback, void* pUserData)
	: Widget(x, y, width, height, &rParent, NumericTextBox::WndProc)
	, m_changedCallback(changedCallback)
	, m_pUserData(pUserData)
{
	// Create child textbox control
	m_hTextBox = CreateWidgetWindow(GetWindowHandle(), "Edit", 0, 0, width, height, 0, WS_EX_CLIENTEDGE);
}

NumericTextBox::~NumericTextBox()
{
	DestroyWindow(m_hTextBox);
}

void NumericTextBox::SetValue(float val, bool triggerCallback)
{
	char text[64];
	sprintf_s(text, "%.4f", val);
	SetWindowTextA(m_hTextBox, text);

	if (triggerCallback && m_changedCallback)
	{
		m_changedCallback(val, m_pUserData);
	}
}

LRESULT CALLBACK NumericTextBox::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		int evt = HIWORD(wParam);
		if (evt == EN_KILLFOCUS)
		{
			char str[256];
			NumericTextBox* pTextBox = (NumericTextBox*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
			GetWindowTextA(pTextBox->m_hTextBox, str, ARRAY_SIZE(str));
			
			float val = (float)atof(str);
			pTextBox->SetValue(val, true);
		}
		break;
	}

	return Widget::WndProc(hWnd, msg, wParam, lParam);
}
