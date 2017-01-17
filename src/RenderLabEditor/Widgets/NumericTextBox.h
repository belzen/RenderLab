#pragma once

#include "Widget.h"

class NumericTextBox : public Widget
{
public:
	typedef bool (*ChangedFunc)(float newValue, void* pUserData);

	static NumericTextBox* Create(const Widget& rParent, int x, int y, int width, int height, 
		ChangedFunc changedCallback, void* pUserData);

public:
	void SetValue(float val, bool triggerCallback);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	NumericTextBox(const Widget& rParent, int x, int y, int width, int height, 
		ChangedFunc changedCallback, void* pUserData);
	NumericTextBox(const NumericTextBox&);

	virtual ~NumericTextBox();

private:
	HWND m_hTextBox;

	ChangedFunc m_changedCallback;
	void* m_pUserData;
};