#include "Precompiled.h"
#include "AssetSelector.h"
#include "UtilsLib/Util.h"
#include "Label.h"
#include "Button.h"
#include <shobjidl.h>
#include "UtilsLib\Paths.h"

namespace
{
	std::string getAssetName(const std::string& srcFilename)
	{
		uint startPos = (uint)srcFilename.find_last_of('\\') + 1;
		uint endPos = (uint)srcFilename.find_last_of('.');
		std::string assetName = srcFilename.substr(startPos, endPos - startPos);
		return assetName;
	}

	void selectFile(void* pUserData)
	{
		char filename[256] = { 0 };

		OPENFILENAMEA ofn = { 0 };
		ofn.lStructSize = sizeof(OPENFILENAMEA);
		// todo: support other asset types.
		ofn.lpstrFilter = "Model (*.modelbin)\0*.modelbin\0\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFile = filename;
		ofn.nMaxFile = ARRAY_SIZE(filename);
		ofn.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameA(&ofn))
		{
			AssetSelector* pSelector = static_cast<AssetSelector*>(pUserData);
			std::string assetName = getAssetName(filename);
			pSelector->SetValue(assetName.c_str(), true);
		}
	}
}

AssetSelector* AssetSelector::Create(const Widget& rParent, int x, int y, int width, int height,
	ChangedFunc changedCallback, void* pUserData)
{
	return new AssetSelector(rParent, x, y, width, height, changedCallback, pUserData);
}

AssetSelector::AssetSelector(const Widget& rParent, int x, int y, int width, int height,
	ChangedFunc changedCallback, void* pUserData)
	: m_changedCallback(changedCallback)
	, m_pUserData(pUserData)
{
	// Create container window
	static bool s_bRegisteredClass = false;
	const char* kContainerClassName = "AssetSelectorContainer";
	if (!s_bRegisteredClass)
	{
		RegisterWindowClass(kContainerClassName, DefWindowProc);
		s_bRegisteredClass = true;
	}

	CreateRootWidgetWindow(rParent.GetWindowHandle(), kContainerClassName, x, y, width, height);

	const uint kButtonSize = 20;
	m_pLabel = Label::Create(*this, 0, 0, width - kButtonSize, height, "Test");
	m_pButton = Button::Create(*this, width - kButtonSize, 0, kButtonSize, kButtonSize, "...", selectFile, this);
}

AssetSelector::~AssetSelector()
{
	m_pLabel->Release();
	m_pButton->Release();
}

void AssetSelector::SetValue(const std::string& val, bool triggerCallback)
{
	m_pLabel->SetText(val.c_str());

	if (triggerCallback && m_changedCallback)
	{
		m_changedCallback(val, m_pUserData);
	}
}
