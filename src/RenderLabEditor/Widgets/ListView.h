#pragma once

#include "Widget.h"

struct ListViewItem
{
	uint64 typeId;
	void* pData;
};

class ListView : public Widget
{
public:
	typedef void(*SelectionChangedFunc)(const ListView* pList, uint selectedIndex, void* pUserData);

	static ListView* Create(const Widget& rParent, int x, int y, int width, int height,
		SelectionChangedFunc selectionChangedCallback, void* pUserData);

	template<typename ItemT>
	void AddItem(char* name, ItemT* pData);
	void RemoveItem(uint index);

	void SelectItem(uint index);
	const ListViewItem* GetItem(uint index) const;

private:
	ListView(const Widget& rParent, int x, int y, int width, int height,
		SelectionChangedFunc selectionChangedCallback, void* pUserData);
	ListView(const ListView&);

	virtual ~ListView();

	void AddItemInternal(char* name, uint64 typeId, void* pItemData);
	void HandleSelectionChanged();

	static LRESULT CALLBACK WinProc(HWND hWnd, uint msg, WPARAM wParam, LPARAM lParam);

private:

	std::vector<ListViewItem> m_items;
	HWND m_hListView;
	uint m_selectedItem;
	SelectionChangedFunc m_selectionChangedCallback;
	void* m_pUserData;
};


template<typename ItemT>
void ListView::AddItem(char* name, ItemT* pItemData)
{
	AddItemInternal(name, typeid(ItemT).hash_code(), static_cast<void*>(pItemData));
}