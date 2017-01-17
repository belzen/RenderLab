#pragma once
#include "Widget.h"

class ComboBox : public Widget
{
public:
	typedef bool (*ChangedFunc)(int newValue, void* pUserData);

	static ComboBox* Create(const Widget& rParent, int x, int y, int width, int height,
		const NameValuePair* aOptions, int numOptions, ChangedFunc changedCallback, void* pUserData);

public:
	void SetValue(const int val, bool triggerCallback);

private:
	ComboBox(const Widget& rParent, int x, int y, int width, int height,
		const NameValuePair* aOptions, int numOptions, ChangedFunc changedCallback, void* pUserData);
	ComboBox(const ComboBox&);

	virtual ~ComboBox();

	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
	HWND m_hComboBox;

	ChangedFunc m_changedCallback;
	void* m_pUserData;

	const NameValuePair* m_aOptions;
	int m_numOptions;
};