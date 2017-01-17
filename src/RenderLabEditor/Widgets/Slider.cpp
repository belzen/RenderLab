#include "Precompiled.h"
#include "Slider.h"
#include "Label.h"
#include "NumericTextBox.h"

Slider* Slider::Create(const Widget& rParent, int x, int y, int width, int height,
	float minVal, float maxVal, float step, bool bEditableText, ChangedFunc changedCallback, void* pUserData)
{
	return new Slider(rParent, x, y, width, height, minVal, maxVal, step, bEditableText, changedCallback, pUserData);
}

Slider::Slider(const Widget& rParent, int x, int y, int width, int height,
	float minVal, float maxVal, float step, bool bEditableText, ChangedFunc changedCallback, void* pUserData)
	: Widget(x, y, width, height, &rParent, 0, Slider::WndProc)
	, m_changedCallback(changedCallback)
	, m_pUserData(pUserData)
	, m_minVal(minVal)
	, m_maxVal(maxVal)
	, m_step(step)
{
	// Create child slider control
	const int kTextWidth = 50;
	const int kSpacing = 2;
	int numSteps = (int)((m_maxVal - m_minVal) / m_step);

	m_hSlider = CreateChildWindow(GetWindowHandle(), TRACKBAR_CLASSA, 0, 0, width - kTextWidth - kSpacing, height, TBS_NOTICKS | TBS_HORZ);
	::SendMessage(m_hSlider, TBM_SETPAGESIZE, false, numSteps / 10);
	::SendMessage(m_hSlider, TBM_SETRANGE, true, MAKELONG(0, numSteps));

	if (bEditableText)
	{
		m_pTextBox = NumericTextBox::Create(*this, width - kTextWidth, 0, kTextWidth, height, OnTextChanged, this);
	}
	else
	{
		m_pLabel = Label::Create(*this, width - kTextWidth, 2, kTextWidth, height, "");
	}
	SetValue(m_minVal, false);
}

Slider::~Slider()
{
	if (m_pTextBox)
	{
		m_pTextBox->Release();
	}

	if (m_pLabel)
	{
		m_pLabel->Release();
	}

	::DestroyWindow(m_hSlider);
}

void Slider::SetValue(float val, bool triggerCallback)
{
	if (val < m_minVal)
	{
		val = m_minVal;
	}
	else if (val > m_maxVal)
	{
		val = m_maxVal;
	}

	int pos = (int)((val - m_minVal) / m_step);
	::SendMessage(m_hSlider, TBM_SETPOS, true, pos);

	if (m_pLabel)
	{
		char text[16];
		sprintf_s(text, "%.3f", val);
		m_pLabel->SetText(text);
	}
	else if (m_pTextBox)
	{
		m_pTextBox->SetValue(val, false);
	}

	if (triggerCallback && m_changedCallback)
	{
		m_changedCallback(val, m_pUserData);
	}
}

bool Slider::OnTextChanged(float newValue, void* pUserData)
{
	Slider* pSlider = static_cast<Slider*>(pUserData);
	pSlider->SetValue(newValue, true);
	return true;
}

LRESULT CALLBACK Slider::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_VSCROLL:
	case WM_HSCROLL:
		int evt = LOWORD(wParam);
		Slider* pSlider = (Slider*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
		int pos = 0;
		if (evt == SB_THUMBTRACK || evt == SB_THUMBPOSITION)
		{
			pos = HIWORD(wParam);
		}
		else
		{
			pos = (int)::SendMessageA(pSlider->m_hSlider, TBM_GETPOS, 0, 0);
		}
		float newVal = pSlider->m_minVal + (pos * pSlider->m_step);
		pSlider->SetValue(newVal, true);
		break;
	}

	return Widget::WndProc(hWnd, msg, wParam, lParam);
}