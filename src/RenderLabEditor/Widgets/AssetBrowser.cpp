#include "Precompiled.h"
#include "AssetBrowser.h"
#include "UtilsLib/Util.h"
#include "AssetLib/ModelAsset.h"
#include "AssetLib/TextureAsset.h"
#include "AssetLib/MaterialAsset.h"
#include "AssetLib/PostProcessEffectsAsset.h"
#include "AssetLib/SkyAsset.h"
#include "Label.h"
#include "Button.h"
#include "Image.h"
#include "Panel.h"
#include <shobjidl.h>
#include "UtilsLib\Paths.h"
#include "UtilsLib\StringCache.h"

AssetBrowser* AssetBrowser::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new AssetBrowser(rParent, x, y, width, height);
}

AssetBrowser::AssetBrowser(const Widget& rParent, int x, int y, int width, int height)
	: Widget(x, y, width, height, &rParent, WS_BORDER | WS_VSCROLL)
{
	SetDataFolder("");
}

AssetBrowser::~AssetBrowser()
{
	Clear();
}

void AssetBrowser::Clear()
{
	for (AssetBrowserItem& rItem : m_items)
	{
		rItem.pLabel->Release();
		rItem.pImage->Release();
		rItem.pPanel->Release();
	}
	m_items.clear();
}

void AssetBrowser::RepositionItems()
{
	const int kPadding = 5;
	int x = kPadding;
	int y = kPadding;
	int w = GetWidth();
	int numRows = 0;
	for (AssetBrowserItem& rItem : m_items)
	{
		int bw = rItem.pPanel->GetWidth();
		if (x + bw > w - kPadding)
		{
			x = kPadding;
			y += kPadding + rItem.pPanel->GetHeight();
			++numRows;
		}

		rItem.pPanel->SetPosition(x, y);
		x += rItem.pPanel->GetWidth() + kPadding;
	}

	if (m_items.empty())
	{
		SetScroll(0, 0);
	}
	else
	{
		SetScroll(numRows, m_items.front().pPanel->GetHeight() + kPadding);
	}
}

void AssetBrowser::OnPathItemDoubleClicked(Widget* pWidget, int button, void* pUserData)
{
	if (button != 0)
		return;

	// Update asset browser path to the selected folder.
	AssetBrowser* pBrowser = static_cast<AssetBrowser*>(pUserData);
	AssetBrowserItem* pItem = nullptr;

	for (AssetBrowserItem& rItem : pBrowser->m_items)
	{
		if (rItem.pPanel == pWidget)
		{
			pItem = &rItem;
			break;
		}
	}

	const std::string& subFolder = pItem->pLabel->GetText();
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

AssetBrowserItem* AssetBrowser::AddItem(const char* label, Icon icon)
{
	const uint kImageWidth = 100;
	const uint kImageHeight = 80;

	AssetBrowserItem item = { 0 };
	item.pPanel = Panel::Create(*this, 0, 0, kImageWidth, kImageHeight + 20);
	item.pPanel->SetBorder(true);
	item.pImage = Image::Create(*item.pPanel, 0, 0, kImageWidth, kImageHeight, icon);
	item.pImage->SetInputEnabled(false);
	item.pLabel = Label::Create(*item.pPanel, 0, kImageHeight, kImageWidth, 20, label, TextAlignment::kCenter);

	if (icon == Icon::kFolder)
	{
		item.isFolder = true;

		// Insert item at the end of the folder list.
		auto iter = m_items.begin();
		while (iter != m_items.end() && iter->isFolder)
		{
			++iter;
		}
		iter = m_items.insert(iter, item);
		return &(*iter);
	}
	else
	{
		m_items.push_back(item);
		return &m_items.back();
	}
}

void AssetBrowser::SetDataFolder(const std::string& dataFolder)
{
	Clear();

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
			if (isDirectory)
			{
				if (_stricmp(filename, ".") == 0 ||
					(_stricmp(filename, "..") == 0 && pBrowser->m_dataFolder.empty()))
				{
					// Ignore directory '.' which is the current directory.
					// Ignore "up one directory" (..) entry if this is the root data directory.
					return;
				}

				AssetBrowserItem* pItem = pBrowser->AddItem(filename, Icon::kFolder);
				pItem->pPanel->SetMouseDoubleClickedCallback(AssetBrowser::OnPathItemDoubleClicked, pBrowser);
			}
			else
			{
				const char* ext = Paths::GetExtension(filename);
				Icon icon;
				WidgetDragDataType dragType;
				AssetLib::AssetDef* pAssetDef = nullptr;

				// TODO: Generate thumbnails for assets instead of common icon
				if (_stricmp(ext, AssetLib::Model::GetAssetDef().GetExt()) == 0)
				{
					pAssetDef = &AssetLib::Model::GetAssetDef();
					icon = Icon::kModel;
					dragType = WidgetDragDataType::kModelAsset;
				}
				else if (_stricmp(ext, AssetLib::Texture::GetAssetDef().GetExt()) == 0)
				{
					pAssetDef = &AssetLib::Texture::GetAssetDef();
					icon = Icon::kTexture;
					dragType = WidgetDragDataType::kTextureAsset;
				}
				else if (_stricmp(ext, AssetLib::Material::GetAssetDef().GetExt()) == 0)
				{
					pAssetDef = &AssetLib::Material::GetAssetDef();
					icon = Icon::kMaterial;
					dragType = WidgetDragDataType::kMaterialAsset;
				}
				else if (_stricmp(ext, AssetLib::PostProcessEffects::GetAssetDef().GetExt()) == 0)
				{
					pAssetDef = &AssetLib::PostProcessEffects::GetAssetDef();
					icon = Icon::kPostProcessEffects;
					dragType = WidgetDragDataType::kPostProcessEffectsAsset;
				}
				else if (_stricmp(ext, AssetLib::Sky::GetAssetDef().GetExt()) == 0)
				{
					pAssetDef = &AssetLib::Sky::GetAssetDef();
					icon = Icon::kSky;
					dragType = WidgetDragDataType::kSkyAsset;
				}

				if (pAssetDef)
				{
					char filenameNoExt[MAX_PATH];
					Paths::GetFilenameNoExtension(filename, filenameNoExt, ARRAY_SIZE(filenameNoExt));
					AssetBrowserItem* pItem = pBrowser->AddItem(filenameNoExt, icon);

					std::string assetPath = pBrowser->m_dataFolder + filename;
					char assetName[AssetLib::AssetDef::kMaxNameLen];
					pAssetDef->ExtractAssetName(assetPath.c_str(), assetName, ARRAY_SIZE(assetName));
					pItem->pPanel->SetDragData(dragType, (void*)CachedString(assetName).getString());
				}
			}
		}, 
		this);

	RepositionItems();
}
