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
	SetDataFolder("");
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

void AssetBrowser::OnPathItemDoubleClicked(Widget* pWidget, int button, void* pUserData)
{
	if (button != 0)
		return;

	AssetBrowser* pBrowser = static_cast<AssetBrowser*>(pUserData);
	Image* pImage = static_cast<Image*>(pWidget);

	const std::string& subFolder = pImage->GetText();
	std::string newFolder = pBrowser->m_dataFolder;

	if (_stricmp(subFolder.c_str(), "..") == 0)
	{
		int i = (int)newFolder.find_last_of("/\\", newFolder.length() - 2);
		newFolder = newFolder.substr(0, max(0, i));
	}
	else
	{
		newFolder += subFolder;
	}

	pBrowser->SetDataFolder(newFolder);
}

void AssetBrowser::SetDataFolder(const std::string& dataFolder)
{
	ClearWidgets();

	m_dataFolder = dataFolder;
	if (!m_dataFolder.empty() && m_dataFolder.back() != '\\')
	{
		m_dataFolder += '\\';
	}

	std::string searchPattern = Paths::GetDataDir() + ("\\" + m_dataFolder + "\\" + "*");
	Paths::ForEachFile(searchPattern.c_str(), false,
		[](const char* filename, bool isDirectory, void* pUserData)
		{
			AssetBrowser* pBrowser = static_cast<AssetBrowser*>(pUserData);
			const uint kButtonSize = 75;
			if (isDirectory)
			{
				if (_stricmp(filename, ".") == 0 ||
					(_stricmp(filename, "..") == 0 && pBrowser->m_dataFolder.empty()))
				{
					// Ignore directory '.' which is the current directory.
					// Ignore "up one directory" entry if this is the root data directory.
					return;
				}

				Image* pImage = Image::Create(*pBrowser, 0, 0, kButtonSize, kButtonSize, filename);
				pImage->SetMouseDoubleClickedCallback(AssetBrowser::OnPathItemDoubleClicked, pBrowser);
				pBrowser->m_widgets.push_back(pImage);
			}
			else
			{
				Image* pImage = Image::Create(*pBrowser, 0, 0, kButtonSize, kButtonSize, filename);
				pBrowser->m_widgets.push_back(pImage);
			}
		}, 
		this);

	RepositionButtons();
}
