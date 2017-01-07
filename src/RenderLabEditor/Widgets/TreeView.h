#pragma once

#include "Widget.h"

class TreeView;

typedef HTREEITEM TreeViewItemHandle;

struct TreeViewItem
{
	uint64 typeId;
	void* pData;
	TreeViewItemHandle hItem;
};

class TreeView : public Widget
{
public:
	static TreeView* Create(const Widget& rParent, int x, int y, int width, int height);

	template<typename ItemT>
	void AddItem(const char* name, ItemT* pData);

	void RemoveItem(TreeViewItemHandle hItem);
	void RemoveItemByData(void* pItemData);

	void SelectItem(void* pItemData);

	const TreeViewItem* GetItem(TreeViewItemHandle hItem) const;

	void Clear();

	typedef void (*SelectionChangedFunc)(const TreeView* pTree, TreeViewItemHandle hSelectedItem, void* pUserData);
	void SetSelectionChangedCallback(SelectionChangedFunc callback, void* pUserData);

	typedef void (*ItemDeletedFunc)(const TreeView* pTree, TreeViewItem* pDeletedItem, void* pUserData);
	void SetItemDeletedCallback(ItemDeletedFunc callback, void* pUserData);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, uint msg, WPARAM wParam, LPARAM lParam);

	TreeView(const Widget& rParent, int x, int y, int width, int height);
	TreeView(const TreeView&);

	virtual ~TreeView();

	void AddItemInternal(const char* name, uint64 typeId, void* pItemData);

	// Input event handling
	void HandleSelectionChanged();

private:
	std::map<TreeViewItemHandle, TreeViewItem> m_items;

	HWND m_hTreeView;
	TreeViewItemHandle m_hSelectedItem;

	SelectionChangedFunc m_selectionChangedCallback;
	void* m_pSelectionChangedUserData;

	ItemDeletedFunc m_itemDeletedCallback;
	void* m_pItemDeletedUserData;
};


template<typename ItemT>
void TreeView::AddItem(const char* name, ItemT* pItemData)
{
	AddItemInternal(name, typeid(ItemT).hash_code(), static_cast<void*>(pItemData));
}