#include "Precompiled.h"
#include "PropertyPanel.h"
#include "WindowBase.h"
#include "Slider.h"
#include "TextBox.h"
#include "NumericTextBox.h"
#include "Label.h"
#include "CheckBox.h"
#include "Button.h"
#include "AssetSelector.h"
#include "PropertyTables.h"
#include "ViewModels/IViewModel.h"

PropertyPanel* PropertyPanel::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new PropertyPanel(rParent, x, y, width, height);
}

PropertyPanel::PropertyPanel(const Widget& rParent, int x, int y, int width, int height)
	: Widget(x, y, width, height, &rParent, WS_BORDER)
	, m_pViewModel(nullptr)
{
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
	int x = 3;
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

		int controlWidth = GetWidth() - x - kPadding;

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
			const TextPropertyDef* pTextDef = (const TextPropertyDef*)pDef;

			TextBox* pTextBox = TextBox::Create(*this, x, y, controlWidth, kRowHeight,
				pTextDef->GetChangedCallback(), pViewModel);

			std::string str = pTextDef->GetValue(pViewModel);
			pTextBox->SetValue(str, false);
			y += pTextBox->GetHeight() + kPadding;

			m_childWidgets.push_back(pTextBox);
		}
		else if (typeId == BooleanPropertyDef::kTypeId)
		{
			const BooleanPropertyDef* pBoolDef = (const BooleanPropertyDef*)pDef;

			CheckBox* pCheckBox = CheckBox::Create(*this, x, y, controlWidth, kRowHeight, pBoolDef->GetChangedCallback(), pViewModel);
			pCheckBox->SetValue(pBoolDef->GetValue(pViewModel), false);
			y += pCheckBox->GetHeight() + kPadding;

			m_childWidgets.push_back(pCheckBox);
		}
		else if (typeId == Vector3PropertyDef::kTypeId)
		{
			const Vector3PropertyDef* pVecDef = (const Vector3PropertyDef*)pDef;

			int elemWidth = controlWidth / 3;
			NumericTextBox* pTextBoxX = NumericTextBox::Create(*this, x, y, elemWidth, kRowHeight, 
				pVecDef->GetXChangedCallback(), pViewModel);
			NumericTextBox* pTextBoxY = NumericTextBox::Create(*this, x + elemWidth, y, elemWidth, kRowHeight,
				pVecDef->GetYChangedCallback(), pViewModel);
			NumericTextBox* pTextBoxZ = NumericTextBox::Create(*this, x + 2 * elemWidth, y, elemWidth, kRowHeight,
				pVecDef->GetZChangedCallback(), pViewModel);

			Vec3 vecValue = pVecDef->GetValue(pViewModel);
			pTextBoxX->SetValue(vecValue.x, false);
			pTextBoxY->SetValue(vecValue.y, false);
			pTextBoxZ->SetValue(vecValue.z, false);

			y += pTextBoxX->GetHeight() + kPadding;

			m_childWidgets.push_back(pTextBoxX);
			m_childWidgets.push_back(pTextBoxY);
			m_childWidgets.push_back(pTextBoxZ);
		}
		else if (typeId == ModelPropertyDef::kTypeId)
		{
			const ModelPropertyDef* pModelDef = (const ModelPropertyDef*)pDef;
			std::string str = pModelDef->GetValue(pViewModel);

			AssetSelector* pSelector = AssetSelector::Create(*this, x, y, controlWidth, kRowHeight, WidgetDragDataType::kModelAsset, pModelDef->GetChangedCallback(), pViewModel);
			pSelector->SetValue(str, false);

			m_childWidgets.push_back(pSelector);

			y += pSelector->GetHeight() + kPadding;
		}
		else
		{
			assert(false);
		}

		x -= kLabelWidth;
		++i;
	}
}
