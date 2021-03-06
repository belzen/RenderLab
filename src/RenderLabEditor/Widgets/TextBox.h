#pragma once

#include "Widget.h"

class TextBox : public Widget
{
public:
	typedef bool (*ChangedFunc)(const std::string& newValue, void* pUserData);

	static TextBox* Create(const Widget& rParent, int x, int y, int width, int height,
		ChangedFunc changedCallback, void* pUserData);

public:
	void SetValue(const std::string& val, bool triggerCallback);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	TextBox(const Widget& rParent, int x, int y, int width, int height,
		ChangedFunc changedCallback, void* pUserData);
	TextBox(const TextBox&);

	virtual ~TextBox();

private:
	HWND m_hTextBox;

	ChangedFunc m_changedCallback;
	void* m_pUserData;
};