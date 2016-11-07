#pragma once
#include "Widget.h"

class CheckBox : public Widget
{
public:
	typedef bool (*ToggledFunc)(bool newState, void* pUserData);

	static CheckBox* Create(const Widget& rParent, int x, int y, int width, int height,
		ToggledFunc toggledCallback, void* pUserData);

public:
	void SetValue(const bool val, bool triggerCallback);

private:
	CheckBox(const Widget& rParent, int x, int y, int width, int height,
		ToggledFunc toggledCallback, void* pUserData);
	CheckBox(const CheckBox&);

	virtual ~CheckBox();

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	HWND m_hCheckBox;
	ToggledFunc m_toggledCallback;
	void* m_pUserData;
};