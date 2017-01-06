#include "Precompiled.h"
#include "AssetSelector.h"
#include "Label.h"

AssetSelector* AssetSelector::Create(const Widget& rParent, int x, int y, int width, int height,
	WidgetDragDataType dataType, AssetChangedFunc changedCallback, void* pUserData)
{
	return new AssetSelector(rParent, x, y, width, height, dataType, changedCallback, pUserData);
}

AssetSelector::AssetSelector(const Widget& rParent, int x, int y, int width, int height,
	WidgetDragDataType dataType, AssetChangedFunc changedCallback, void* pUserData)
	: Widget(x, y, width, height, &rParent, WS_BORDER)
	, m_dataType(dataType)
	, m_changedCallback(changedCallback)
	, m_pUserData(pUserData)
{
	m_pLabel = Label::Create(*this, 0, 0, width, height, "");
}

AssetSelector::~AssetSelector()
{
	m_pLabel->Release();
}

void AssetSelector::SetValue(const std::string& val, bool triggerCallback)
{
	m_pLabel->SetText(val.c_str());

	if (triggerCallback && m_changedCallback)
	{
		m_changedCallback(val, m_pUserData);
	}
}

void AssetSelector::OnDragEnd(Widget* pDraggedWidget)
{
	const WidgetDragData& rDragData = pDraggedWidget->GetDragData();
	if (rDragData.type == m_dataType)
	{
		SetValue(rDragData.data.assetName, true);
	}
}