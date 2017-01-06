#pragma once
#include "Widget.h"

class Label;
class Button;

class AssetSelector : public Widget
{
public:
	typedef bool (*AssetChangedFunc)(const std::string& newValue, void* pUserData);

	static AssetSelector* Create(const Widget& rParent, int x, int y, int width, int height,
		WidgetDragDataType dataType, AssetChangedFunc changedCallback, void* pUserData);

public:
	void SetValue(const std::string& val, bool triggerCallback);

private:
	AssetSelector(const Widget& rParent, int x, int y, int width, int height,
		WidgetDragDataType dataType, AssetChangedFunc changedCallback, void* pUserData);
	AssetSelector(const AssetSelector&);

	virtual ~AssetSelector();

	void OnDragEnd(Widget* pDraggedWidget);

private:
	Label* m_pLabel;

	WidgetDragDataType m_dataType;

	AssetChangedFunc m_changedCallback;
	void* m_pUserData;
};