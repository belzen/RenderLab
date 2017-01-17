#include "Precompiled.h"
#include "PropertyPanel.h"
#include "WindowBase.h"
#include "Slider.h"
#include "TextBox.h"
#include "NumericTextBox.h"
#include "Label.h"
#include "CheckBox.h"
#include "ComboBox.h"
#include "Button.h"
#include "Separator.h"
#include "AssetSelector.h"
#include "PropertyTables.h"
#include "ViewModels/IViewModel.h"

PropertyPanel* PropertyPanel::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new PropertyPanel(rParent, x, y, width, height);
}

PropertyPanel::PropertyPanel(const Widget& rParent, int x, int y, int width, int height)
	: Widget(x, y, width, height, &rParent, WS_BORDER | WS_VSCROLL)
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

void PropertyPanel::SetViewModel(IViewModel* pViewModel, bool freeOldViewModels)
{
	// Free old widgets
	DestroyChildWidgets();

	if (freeOldViewModels)
	{
		for (IViewModel* pViewModel : m_viewModels)
		{
			delete pViewModel;
		}
	}

	m_viewModels.clear();
	AddViewModel(pViewModel);
}

void PropertyPanel::AddViewModel(IViewModel* pViewModel)
{
	const int kPadding = 5;
	const int kIndent = 10;
	const int kRowHeight = 25;
	const int kLabelWidth = 100;
	const int kLabelOffsetY = 3;
	const int kRowWidth = GetWidth() - 15;
	int x = 3;
	int y = 2;
	int i = 0;

	m_viewModels.push_back(pViewModel);

	if (!m_childWidgets.empty())
	{
		Widget* pLast = m_childWidgets.back();
		y = pLast->GetY() + pLast->GetHeight();

		// Separator between view models
		y += kPadding;
		m_childWidgets.push_back(Separator::Create(*this, 0, y, kRowWidth, 1));

		// Header label.  Only used for the second view model and on.
		char header[64];
		sprintf_s(header, "--%s--", pViewModel->GetTypeName());

		Label* pHeaderLabel = Label::Create(*this, 10, y + 2, kRowWidth, 14, header);
		m_childWidgets.push_back(pHeaderLabel);
		y += pHeaderLabel->GetHeight() + kPadding;
	}

	// Create widgets for the new view model.
	const PropertyDef** ppProperties = pViewModel->GetProperties();
	while (ppProperties[i])
	{
		const PropertyDef* pDef = ppProperties[i];
		const std::type_info& rTypeInfo = typeid(*pDef);

		if (rTypeInfo == typeid(EndGroupPropertyDef))
		{
			x -= kIndent;
		}
		else
		{ 
			Label* pLabel = Label::Create(*this, x, y + kLabelOffsetY, kLabelWidth, kRowHeight, pDef->GetName());
			m_childWidgets.push_back(pLabel);
			x += kLabelWidth;

			int controlWidth = kRowWidth - x - kPadding;

			if (rTypeInfo == typeid(BeginGroupPropertyDef))
			{
				x += kIndent;
				y += pLabel->GetHeight();
			}
			else if (rTypeInfo == typeid(FloatPropertyDef))
			{
				const FloatPropertyDef* pFloatDef = (const FloatPropertyDef*)pDef;
				Slider* pSlider = Slider::Create(*this, x, y, controlWidth, kRowHeight,
					pFloatDef->GetMinValue(), pFloatDef->GetMaxValue(), pFloatDef->GetStep(), true, pFloatDef->GetChangedCallback(), pViewModel);
				pSlider->SetValue(pFloatDef->GetValue(pViewModel), false);
				y += pSlider->GetHeight();

				m_childWidgets.push_back(pSlider);
			}
			else if (rTypeInfo == typeid(IntegerPropertyDef))
			{
				/*
				const IntegerPropertyDef* pIntDef = (const IntegerPropertyDef*)pDef;
				Slider* pSlider = Slider::Create(*this, x, y, controlWidth, kRowHeight,
					pIntDef->GetMinValue(), pIntDef->GetMaxValue(), pIntDef->GetStep(), pIntDef->GetChangedCallback(), pData);
				y += pSlider->GetHeight() + kPadding;
				*/
			}
			else if (rTypeInfo == typeid(TextPropertyDef))
			{
				const TextPropertyDef* pTextDef = (const TextPropertyDef*)pDef;

				TextBox* pTextBox = TextBox::Create(*this, x, y, controlWidth, kRowHeight,
					pTextDef->GetChangedCallback(), pViewModel);

				std::string str = pTextDef->GetValue(pViewModel);
				pTextBox->SetValue(str, false);
				y += pTextBox->GetHeight();

				m_childWidgets.push_back(pTextBox);
			}
			else if (rTypeInfo == typeid(BooleanPropertyDef))
			{
				const BooleanPropertyDef* pBoolDef = (const BooleanPropertyDef*)pDef;

				CheckBox* pCheckBox = CheckBox::Create(*this, x, y, controlWidth, kRowHeight, pBoolDef->GetChangedCallback(), pViewModel);
				pCheckBox->SetValue(pBoolDef->GetValue(pViewModel), false);
				y += pCheckBox->GetHeight();

				m_childWidgets.push_back(pCheckBox);
			}
			else if (rTypeInfo == typeid(IntChoicePropertyDef))
			{
				const IntChoicePropertyDef* pChoiceDef = (const IntChoicePropertyDef*)pDef;

				ComboBox* pComboBox = ComboBox::Create(*this, x, y, controlWidth, kRowHeight, pChoiceDef->GetOptions(), pChoiceDef->GetNumOptions(), pChoiceDef->GetChangedCallback(), pViewModel);
				pComboBox->SetValue(pChoiceDef->GetValue(pViewModel), false);
				y += pComboBox->GetHeight();

				m_childWidgets.push_back(pComboBox);
			}
			else if (rTypeInfo == typeid(Vector3PropertyDef))
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

				y += pTextBoxX->GetHeight();

				m_childWidgets.push_back(pTextBoxX);
				m_childWidgets.push_back(pTextBoxY);
				m_childWidgets.push_back(pTextBoxZ);
			}
			else if (rTypeInfo == typeid(AssetPropertyDef))
			{
				const AssetPropertyDef* pAssetDef = (const AssetPropertyDef*)pDef;
				std::string str = pAssetDef->GetValue(pViewModel);

				AssetSelector* pSelector = AssetSelector::Create(*this, x, y, controlWidth, kRowHeight, pAssetDef->GetAssetType(), pAssetDef->GetChangedCallback(), pViewModel);
				pSelector->SetValue(str, false);

				m_childWidgets.push_back(pSelector);

				y += pSelector->GetHeight();
			}
			else
			{
				assert(false);
			}

			y += kPadding;
			x -= kLabelWidth;
		}

		++i;
	}

	// Update scroll
	if (!m_childWidgets.empty())
	{
		Widget* pLast = m_childWidgets.back();
		y = pLast->GetY() + pLast->GetHeight() + kPadding;
		if (y > GetHeight())
		{
			SetScroll(y, 1, GetHeight());
		}
		else
		{
			SetScroll(0, 0, 1);
		}
	}
}
