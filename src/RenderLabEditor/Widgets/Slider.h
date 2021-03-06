#pragma once

#include <windows.h>
#include "Widget.h"

class Label;
class NumericTextBox;

class Slider : public Widget
{
public:
	typedef bool (*ChangedFunc)(float newValue, void* pUserData);

	static Slider* Create(const Widget& rParent, int x, int y, int width, int height, 
		float minVal, float maxVal, float step, bool bEditableText, ChangedFunc changedCallback, void* pUserData);

public:
	void SetValue(float val, bool triggerCallback);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static bool OnTextChanged(float newValue, void* pUserData);

	Slider(const Widget& rParent, int x, int y, int width, int height,
		float minVal, float maxVal, float step, bool bEditableText, ChangedFunc changedCallback, void* pUserData);
	Slider(const Slider&);

	virtual ~Slider();

private:
	HWND m_hSlider;
	Label* m_pLabel;
	NumericTextBox* m_pTextBox;
	ChangedFunc m_changedCallback;
	void* m_pUserData;
	float m_minVal;
	float m_maxVal;
	float m_step;
};