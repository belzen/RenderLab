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
	void SetDataFolder(const std::string& folderPath);

private:
	AssetBrowser(const Widget& rParent, int x, int y, int width, int height);
	AssetBrowser(const AssetBrowser&);

	virtual ~AssetBrowser();

	void ClearWidgets();
	void RepositionButtons();

	static void OnPathItemDoubleClicked(Widget* pWidget, int button, void* pUserData);

private:
	std::vector<Widget*> m_widgets;
	std::string m_dataFolder;
};