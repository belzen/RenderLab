#pragma once
#include "Widget.h"

class Label;
class Button;

class AssetSelector : public Widget
{
public:
	typedef bool (*ChangedFunc)(const std::string& newValue, void* pUserData);

	static AssetSelector* Create(const Widget& rParent, int x, int y, int width, int height,
		ChangedFunc changedCallback, void* pUserData);

public:
	void SetValue(const std::string& val, bool triggerCallback);

private:
	AssetSelector(const Widget& rParent, int x, int y, int width, int height,
		ChangedFunc changedCallback, void* pUserData);
	AssetSelector(const AssetSelector&);

	virtual ~AssetSelector();

private:
	Label* m_pLabel;
	Button* m_pButton;

	ChangedFunc m_changedCallback;
	void* m_pUserData;
};