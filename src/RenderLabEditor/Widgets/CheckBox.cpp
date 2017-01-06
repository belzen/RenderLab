#include "Precompiled.h"
#include "CheckBox.h"
#include "Types.h"

CheckBox* CheckBox::Create(const Widget& rParent, int x, int y, int width, int height,
	ToggledFunc toggledCallback, void* pUserData)
{
	return new CheckBox(rParent, x, y, width, height, toggledCallback, pUserData);
}

CheckBox::CheckBox(const Widget& rParent, int x, int y, int width, int height,
	ToggledFunc toggledCallback, void* pUserData)
	: Widget(x, y, width, height, &rParent, 0, CheckBox::WndProc)
	, m_pUserData(pUserData)
{
	// Create check box control
	m_hCheckBox = CreateChildWindow(GetWindowHandle(), "Button", 0, 0, width, height, BS_CHECKBOX);
	m_toggledCallback = toggledCallback;
}

CheckBox::~CheckBox()
{
	DestroyWindow(m_hCheckBox);
}

void CheckBox::SetValue(const bool val, bool triggerCallback)
{
	CheckDlgButton(GetWindowHandle(), 0, val ? BST_CHECKED : BST_UNCHECKED);

	if (triggerCallback && m_toggledCallback)
	{
		m_toggledCallback(val, m_pUserData);
	}
}

LRESULT CALLBACK CheckBox::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		CheckBox* pCheckBox = (CheckBox*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
		uint checkedState = IsDlgButtonChecked(pCheckBox->GetWindowHandle(), 0);
		pCheckBox->SetValue(checkedState != BST_CHECKED, true);
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}