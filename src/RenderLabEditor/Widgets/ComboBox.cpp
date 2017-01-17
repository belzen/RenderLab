#include "Precompiled.h"
#include "ComboBox.h"

ComboBox* ComboBox::Create(const Widget& rParent, int x, int y, int width, int height,
	const NameValuePair* aOptions, int numOptions, ChangedFunc changedCallback, void* pUserData)
{
	return new ComboBox(rParent, x, y, width, height, aOptions, numOptions, changedCallback, pUserData);
}

ComboBox::ComboBox(const Widget& rParent, int x, int y, int width, int height,
	const NameValuePair* aOptions, int numOptions, ChangedFunc changedCallback, void* pUserData)
	: Widget(x, y, width, height, &rParent, 0, ComboBox::WndProc)
	, m_changedCallback(changedCallback)
	, m_pUserData(pUserData)
	, m_aOptions(aOptions)
	, m_numOptions(numOptions)
{
	// Max height for the drop-down list when the combo box is clicked on.
	// If the list grows beyond this height, it will scroll.
	const int kDropDownMaxHeight = 150;

	// Create combo box control
	m_hComboBox = CreateChildWindow(GetWindowHandle(), WC_COMBOBOXA, 0, 0, width, kDropDownMaxHeight, CBS_DROPDOWNLIST | CBS_HASSTRINGS | WS_VSCROLL);
	for (int i = 0; i < m_numOptions; ++i)
	{
		ComboBox_AddString(m_hComboBox, m_aOptions[i].name);
	}
}

ComboBox::~ComboBox()
{
	::DestroyWindow(m_hComboBox);
}

void ComboBox::SetValue(const int val, bool triggerCallback)
{
	for (int i = 0; i < m_numOptions; ++i)
	{
		if (m_aOptions[i].value == val)
		{
			ComboBox_SetCurSel(m_hComboBox, i);
			break;
		}
	}

	if (triggerCallback && m_changedCallback)
	{
		m_changedCallback(val, m_pUserData);
	}
}

LRESULT CALLBACK ComboBox::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		if (HIWORD(wParam) == CBN_SELCHANGE)
		{
			ComboBox* pComboBox =  (ComboBox*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
			int selected = ComboBox_GetCurSel((HWND)lParam);
			pComboBox->SetValue(pComboBox->m_aOptions[selected].value, true);
		}
		break;
	}

	return Widget::WndProc(hWnd, msg, wParam, lParam);
}