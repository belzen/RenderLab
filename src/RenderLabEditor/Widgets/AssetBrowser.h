#pragma once
#include "Widget.h"
#include <vector>

class Button;

class AssetBrowser : public Widget
{
public:
	typedef bool(*ChangedFunc)(const std::string& newValue, void* pUserData);

	static AssetBrowser* Create(const Widget& rParent, int x, int y, int width, int height);

public:
	void SetPath(const std::string& path);

private:
	AssetBrowser(const Widget& rParent, int x, int y, int width, int height);
	AssetBrowser(const AssetBrowser&);

	virtual ~AssetBrowser();

	void ClearWidgets();
	void RepositionButtons();

	static void SelectAsset(void* pUserData);
	static void SelectPath(void* pUserData);

private:
	std::vector<Widget*> m_widgets;
	std::string m_path;
};