#include "Precompiled.h"
#include "Menu.h"
#include "Types.h"

namespace
{
	uint s_lastMenuCommandId = 1;
}

Menu::~Menu()
{
	::DestroyMenu(m_hMenu);
}

void Menu::Init()
{
	m_hMenu = ::CreateMenu();
}

void Menu::InitAsContextMenu()
{
	m_hMenu = ::CreatePopupMenu();
}

void Menu::AddItem(const char* text, MenuCommandCallback callbackFunc, void* pUserData)
{
	MenuItem item = { 0 };
	item.callbackFunc = callbackFunc;
	item.pUserData = pUserData;
	item.commandId = ::InterlockedIncrement(&s_lastMenuCommandId);
	m_items.push_back(item);

	::AppendMenuA(m_hMenu, MF_STRING, (UINT_PTR)item.commandId, text);
}

void Menu::AddSubMenu(const char* name, Menu* pMenu)
{
	MenuItem item = { 0 };
	item.pSubMenu = pMenu;
	m_items.push_back(item);

	::AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)pMenu->m_hMenu, name);
}

void Menu::SetItemChecked(int itemIndex, bool check)
{
	int stateFlag = check ? MF_CHECKED : MF_UNCHECKED;
	CheckMenuItem(m_hMenu, itemIndex, MF_BYPOSITION | stateFlag);
}

void Menu::SetRadioItemChecked(int itemIndex)
{
	CheckMenuRadioItem(m_hMenu, 0, (int)m_items.size() - 1, itemIndex, MF_BYPOSITION);
}

bool Menu::HandleMenuCommand(int commandId)
{
	int numItems = (int)m_items.size();
	for (int i = 0; i < numItems; ++i)
	{
		MenuItem& rItem = m_items[i];
		if (rItem.commandId == commandId)
		{
			rItem.callbackFunc(rItem.pUserData);
			return true;
		}
		else if (rItem.pSubMenu && rItem.pSubMenu->HandleMenuCommand(commandId))
		{
			return true;
		}
	}

	return false;
}

