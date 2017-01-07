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
	typedef void(*SelectionChangedFunc)(const TreeView* pList, TreeViewItemHandle hSelectedItem, void* pUserData);

	static TreeView* Create(const Widget& rParent, int x, int y, int width, int height,
		SelectionChangedFunc selectionChangedCallback, void* pUserData);

	template<typename ItemT>
	void AddItem(const char* name, ItemT* pData);

	void RemoveItem(void* pItemData);

	void SelectItem(void* pItemData);

	const TreeViewItem* GetItem(TreeViewItemHandle hItem) const;

	void Clear();

public:
	TreeView(const Widget& rParent, int x, int y, int width, int height,
		SelectionChangedFunc selectionChangedCallback, void* pUserData);
	TreeView(const TreeView&);

	virtual ~TreeView();

	void AddItemInternal(const char* name, uint64 typeId, void* pItemData);
	void HandleSelectionChanged();

	static LRESULT CALLBACK WndProc(HWND hWnd, uint msg, WPARAM wParam, LPARAM lParam);

private:
	std::map<TreeViewItemHandle, TreeViewItem> m_items;

	HWND m_hTreeView;
	TreeViewItemHandle m_hSelectedItem;

	SelectionChangedFunc m_selectionChangedCallback;
	void* m_pSelectionChangedUserData;
};


template<typename ItemT>
void TreeView::AddItem(const char* name, ItemT* pItemData)
{
	AddItemInternal(name, typeid(ItemT).hash_code(), static_cast<void*>(pItemData));
}