#pragma once

typedef void (*MenuCommandCallback)(void* pUserData);

class Menu
{
public:
	~Menu();

	void Init();
	void InitAsContextMenu();

	void AddItem(const char* text, MenuCommandCallback callbackFunc, void* pUserData);
	void AddSubMenu(const char* name, Menu* pMenu);

	void SetItemChecked(int itemIndex, bool check);
	void SetRadioItemChecked(int itemIndex);

	bool HandleMenuCommand(int commandId);

	HMENU GetMenuHandle();

private:
	struct MenuItem
	{
		Menu* pSubMenu;
		MenuCommandCallback callbackFunc;
		int commandId;
		void* pUserData;
	};

	std::vector<MenuItem> m_items;
	HMENU m_hMenu;
};

inline HMENU Menu::GetMenuHandle()
{
	return m_hMenu;
}