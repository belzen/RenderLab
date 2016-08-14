#include "Precompiled.h"
#include "PropertyPanel.h"
#include "WindowBase.h"
#include "Slider.h"
#include "TextBox.h"
#include "Label.h"
#include "CheckBox.h"
#include "Button.h"
#include "PropertyTables.h"
#include "ViewModels/IViewModel.h"

PropertyPanel* PropertyPanel::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new PropertyPanel(rParent, x, y, width, height);
}

PropertyPanel::PropertyPanel(const Widget& rParent, int x, int y, int width, int height)
	: m_pViewModel(nullptr)
{
	const char* kWndClassName = "PropertyPanel";
	static bool s_bRegisteredClass = false;
	if (!s_bRegisteredClass)
	{
		RegisterWindowClass(kWndClassName, DefWindowProc);
		s_bRegisteredClass = true;
	}

	CreateRootWidgetWindow(rParent.GetWindowHandle(), kWndClassName, x, y, width, height);
}

PropertyPanel::~PropertyPanel()
{
	DestroyChildWidgets();
}

void PropertyPanel::DestroyChildWidgets()
{
	for (Widget* pWidget : m_childWidgets)
	{
		pWidget->Release();
	}
	m_childWidgets.clear();
}

void PropertyPanel::SetViewModel(IViewModel* pViewModel, bool freeOldViewModel)
{
	const int kPadding = 5;
	const int kIndent = 10;
	const int kRowHeight = 30;
	const int kLabelWidth = 100;
	int x = 0;
	int y = 0;
	int i = 0;

	// Free old widgets
	DestroyChildWidgets();

	if (freeOldViewModel && m_pViewModel)
	{
		delete m_pViewModel;
	}

	// Create widgets for the new view model.
	m_pViewModel = pViewModel;
	const PropertyDef** ppProperties = pViewModel->GetProperties();
	while (ppProperties[i])
	{
		const PropertyDef* pDef = ppProperties[i];
		uint64 typeId = pDef->GetTypeId();
		
		Label* pLabel = Label::Create(*this, x, y, kLabelWidth, kRowHeight, pDef->GetName());
		m_childWidgets.push_back(pLabel);
		x += kLabelWidth;

		int controlWidth = m_width - kLabelWidth;

		if (typeId == BeginGroupPropertyDef::kTypeId)
		{
			x += kIndent;
			y += pLabel->GetHeight() + kPadding;
		}
		else if (typeId == EndGroupPropertyDef::kTypeId)
		{
			x -= kIndent;
		}
		else if (typeId == FloatPropertyDef::kTypeId)
		{
			const FloatPropertyDef* pFloatDef = (const FloatPropertyDef*)pDef;
			Slider* pSlider = Slider::Create(*this, x, y, controlWidth, kRowHeight,
				pFloatDef->GetMinValue(), pFloatDef->GetMaxValue(), pFloatDef->GetStep(), pFloatDef->GetChangedCallback(), pViewModel);
			pSlider->SetValue(pFloatDef->GetValue(pViewModel), false);
			y += pSlider->GetHeight() + kPadding;

			m_childWidgets.push_back(pSlider);
		}
		else if (typeId == IntegerPropertyDef::kTypeId)
		{
			/*
			const IntegerPropertyDef* pIntDef = (const IntegerPropertyDef*)pDef;
			Slider* pSlider = Slider::Create(*this, x, y, controlWidth, kRowHeight,
				pIntDef->GetMinValue(), pIntDef->GetMaxValue(), pIntDef->GetStep(), pIntDef->GetChangedCallback(), pData);
			y += pSlider->GetHeight() + kPadding;
			*/
		}
		else if (typeId == TextPropertyDef::kTypeId)
		{
			TextBox* pTextBox = TextBox::Create(*this, x, y, controlWidth, kRowHeight);
			y += pTextBox->GetHeight() + kPadding;

			m_childWidgets.push_back(pTextBox);
		}
		else if (typeId == BooleanPropertyDef::kTypeId)
		{
			CheckBox* pCheckBox = CheckBox::Create(*this, x, y, controlWidth, kRowHeight, nullptr);
			y += pCheckBox->GetHeight() + kPadding;

			m_childWidgets.push_back(pCheckBox);
		}
		else if (typeId == Vector3PropertyDef::kTypeId)
		{
			Label* pLabel = Label::Create(*this, x, y, controlWidth, kRowHeight, pDef->GetName());
			x += kIndent;
			y += pLabel->GetHeight() + kPadding;

			m_childWidgets.push_back(pLabel);
		}

		x -= kLabelWidth;
		++i;
	}
}
