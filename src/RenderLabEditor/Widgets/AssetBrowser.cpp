#include "Precompiled.h"
#include "AssetBrowser.h"
#include "UtilsLib/Util.h"
#include "Label.h"
#include "Button.h"
#include "Image.h"
#include <shobjidl.h>
#include "UtilsLib\Paths.h"

AssetBrowser* AssetBrowser::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new AssetBrowser(rParent, x, y, width, height);
}

AssetBrowser::AssetBrowser(const Widget& rParent, int x, int y, int width, int height)
	:Widget(x, y, width, height, &rParent)
{
}

AssetBrowser::~AssetBrowser()
{
	ClearWidgets();
}

void AssetBrowser::ClearWidgets()
{
	for (Widget* pWidget : m_widgets)
	{
		pWidget->Release();
	}
	m_widgets.clear();
}

void AssetBrowser::RepositionButtons()
{
	const int kPadding = 5;
	int x = kPadding;
	int y = kPadding;
	int w = GetWidth();
	for (Widget* pWidget : m_widgets)
	{
		int bw = pWidget->GetWidth();
		if (x + bw > w - kPadding)
		{
			x = kPadding;
			y += pWidget->GetHeight();
		}

		pWidget->SetPosition(x, y);
		x += pWidget->GetWidth() + kPadding;
	}
}

void AssetBrowser::SelectAsset(void* pUserData)
{
	// todo2
}

void AssetBrowser::SelectPath(void* pUserData)
{
	// todo2
}

void AssetBrowser::SetPath(const std::string& path)
{
	m_path = path + "\\";

	ClearWidgets();

	std::string searchPattern = m_path + "*";
	Paths::ForEachFile(searchPattern.c_str(), false,
		[](const char* filename, bool isDirectory, void* pUserData) 
		{
			AssetBrowser* pBrowser = static_cast<AssetBrowser*>(pUserData);
			const uint kButtonSize = 75;
			if (isDirectory)
			{
				pBrowser->m_widgets.push_back(Image::Create(*pBrowser, 0, 0, kButtonSize, kButtonSize, filename));
			}
			else
			{
				pBrowser->m_widgets.push_back(Button::Create(*pBrowser, 0, 0, kButtonSize, kButtonSize, filename, AssetBrowser::SelectAsset, pBrowser));
			}
		}, 
		this);

	RepositionButtons();
}
