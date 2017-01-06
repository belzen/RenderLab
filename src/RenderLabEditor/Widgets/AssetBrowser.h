#pragma once
#include "Widget.h"
#include "IconLibrary.h"
#include <vector>

class Image;
class Label;
class Panel;

struct AssetBrowserItem
{
	Panel* pPanel;
	Image* pImage;
	Label* pLabel;
	bool isFolder;
};

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

	void Clear();
	void RepositionItems();
	AssetBrowserItem* AddItem(const char* label, Icon icon);

	static void OnPathItemDoubleClicked(Widget* pWidget, int button, void* pUserData);

private:
	std::vector<AssetBrowserItem> m_items;
	std::string m_dataFolder;
};