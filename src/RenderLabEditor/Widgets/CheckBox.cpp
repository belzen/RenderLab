#include "Precompiled.h"
#include "CheckBox.h"
#include "Types.h"

CheckBox* CheckBox::Create(const Widget& rParent, int x, int y, int width, int height,
	ToggledFunc toggledCallback)
{
	return new CheckBox(rParent, x, y, width, height, toggledCallback);
}

CheckBox::CheckBox(const Widget& rParent, int x, int y, int width, int height,
	ToggledFunc toggledCallback)
{
	HMODULE hInstance = ::GetModuleHandle(NULL);

	// Create container
	static bool s_bRegisteredClass = false;
	const char* kContainerClassName = "CheckBoxContainer";
	if (!s_bRegisteredClass)
	{
		RegisterWindowClass(kContainerClassName, CheckBox::WndProc);
		s_bRegisteredClass = true;
	}
	CreateRootWidgetWindow(rParent.GetWindowHandle(), kContainerClassName, x, y, width, height);

	// Create check box control
	m_hCheckBox = CreateWidgetWindow(GetWindowHandle(), "Button", 0, 0, width, height, BS_CHECKBOX);
	m_toggledCallback = toggledCallback;
}

CheckBox::~CheckBox()
{
	DestroyWindow(m_hCheckBox);
}

LRESULT CALLBACK CheckBox::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_COMMAND:
		CheckBox* pCheckBox = (CheckBox*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
		uint checkedState = IsDlgButtonChecked(pCheckBox->GetWindowHandle(), 0);
		if (checkedState == BST_CHECKED)
		{
			CheckDlgButton(hWnd, 0, BST_UNCHECKED);
		}
		else
		{
			CheckDlgButton(hWnd, 0, BST_CHECKED);
		}

		if (pCheckBox->m_toggledCallback)
		{
			pCheckBox->m_toggledCallback(pCheckBox, checkedState != BST_CHECKED);
		}
		
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}