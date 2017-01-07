#include "Precompiled.h"
#include "TreeView.h"
#include "input/Input.h"

TreeView* TreeView::Create(const Widget& rParent, int x, int y, int width, int height)
{
	return new TreeView(rParent, x, y, width, height);
}

TreeView::TreeView(const Widget& rParent, int x, int y, int width, int height)
	: Widget(x, y, width, height, &rParent, WS_BORDER, TreeView::WndProc)
	, m_hSelectedItem(TVI_ROOT)
{
	m_hTreeView = CreateChildWindow(GetWindowHandle(), WC_TREEVIEWA, 0, 0, width, height,
		TVS_FULLROWSELECT | TVS_SHOWSELALWAYS | TVS_HASBUTTONS | TVS_EDITLABELS, 0);
}

TreeView::~TreeView()
{
	::DestroyWindow(m_hTreeView);
}

void TreeView::SetSelectionChangedCallback(SelectionChangedFunc callback, void* pUserData)
{
	m_selectionChangedCallback = callback;
	m_pSelectionChangedUserData = pUserData;
}

void TreeView::SetItemDeletedCallback(ItemDeletedFunc callback, void* pUserData)
{
	m_itemDeletedCallback = callback;
	m_pItemDeletedUserData = pUserData;
}

void TreeView::RemoveItem(TreeViewItemHandle hItem)
{
	auto iter = m_items.find(hItem);
	if (iter == m_items.end())
		return;

	// Copy item before removing it to use in the callback.
	TreeViewItem item = iter->second;
	TreeView_DeleteItem(m_hTreeView, hItem);
	m_items.erase(iter);

	// Issue callback after deleting the item.
	if (m_itemDeletedCallback)
	{
		m_itemDeletedCallback(this, &item, m_pItemDeletedUserData);
	}
}

void TreeView::RemoveItemByData(void* pItemData)
{
	for (auto iter = m_items.begin(); iter != m_items.end(); ++iter)
	{
		if (iter->second.pData == pItemData)
		{
			RemoveItem(iter->second.hItem);
			break;
		}
	}
}

void TreeView::Clear()
{
	TreeView_DeleteAllItems(m_hTreeView);
	m_items.clear();
}

void TreeView::SelectItem(void* pItemData)
{
	int i = 0;
	for (auto iter = m_items.begin(); iter != m_items.end(); ++iter, ++i)
	{
		if (iter->second.pData == pItemData)
		{
			TreeView_SelectItem(m_hTreeView, iter->second.hItem);
			break;
		}
	}
}

const TreeViewItem* TreeView::GetItem(TreeViewItemHandle hItem) const
{
	auto iter = m_items.find(hItem);
	return (iter == m_items.end()) ? nullptr : &iter->second;
}

void TreeView::AddItemInternal(const char* name, uint64 typeId, void* pItemData)
{
	TVINSERTSTRUCTA tvInsert = { 0 };
	tvInsert.hParent = TVI_ROOT;
	tvInsert.hInsertAfter = TVI_LAST;
	tvInsert.item.mask = TVIF_TEXT;
	tvInsert.item.lParam = 1;
	tvInsert.item.pszText = const_cast<char*>(name);
	tvInsert.item.cchTextMax = (uint)strlen(tvInsert.item.pszText) + 1;

	TreeViewItem item = { 0 };
	item.typeId = typeId;
	item.pData = pItemData;
	item.hItem = (HTREEITEM)::SendMessageA(m_hTreeView, TVM_INSERTITEMA, 0, (LPARAM)&tvInsert);
	
	m_items.insert(std::make_pair(item.hItem, item));
}

void TreeView::HandleSelectionChanged()
{
	m_hSelectedItem = TreeView_GetSelection(m_hTreeView);
	if (m_selectionChangedCallback)
	{
		m_selectionChangedCallback(this, m_hSelectedItem, m_pSelectionChangedUserData);
	}
}

LRESULT CALLBACK TreeView::WndProc(HWND hWnd, uint msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_NOTIFY:
		TreeView* pTreeView = (TreeView*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
		int notifyCode = ((LPNMHDR)lParam)->code;
		if (notifyCode == TVN_SELCHANGEDA)
		{
			pTreeView->HandleSelectionChanged();
		}
		else if (notifyCode == TVN_KEYDOWN)
		{
			LPNMTVKEYDOWN pEvent = (LPNMTVKEYDOWN)lParam;
			if (pEvent->wVKey == KEY_DELETE)
			{
				pTreeView->RemoveItem(pTreeView->m_hSelectedItem);
			}
		}

		break;
	}

	return Widget::WndProc(hWnd, msg, wParam, lParam);
}