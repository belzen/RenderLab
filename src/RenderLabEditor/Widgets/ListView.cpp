#include "Precompiled.h"
#include "ListView.h"

ListView* ListView::Create(const Widget& rParent, int x, int y, int width, int height,
	SelectionChangedFunc selectionChangedCallback, void* pUserData)
{
	return new ListView(rParent, x, y, width, height, selectionChangedCallback, pUserData);
}

ListView::ListView(const Widget& rParent, int x, int y, int width, int height,
	SelectionChangedFunc selectionChangedCallback, void* pUserData)
	: m_selectedItem(-1)
	, m_selectionChangedCallback(selectionChangedCallback)
	, m_pUserData(pUserData)
{
	const char* kWndClassName = "ListViewContainer";
	static bool s_bRegisteredClass = false;
	if (!s_bRegisteredClass)
	{
		RegisterWindowClass(kWndClassName, ListView::WinProc);
		s_bRegisteredClass = true;
	}

	CreateRootWidgetWindow(rParent.GetWindowHandle(), kWndClassName, x, y, width, height, 0, 0);

	m_hListView = CreateWidgetWindow(GetWindowHandle(), WC_LISTVIEWA, 0, 0, width, height, 
		LVS_NOCOLUMNHEADER | LVS_SHOWSELALWAYS | LVS_REPORT | LVS_EDITLABELS, 0);
	
	LV_COLUMNA column = { 0 };
	column.mask = LVCF_WIDTH;
	column.cx = width;
	::SendMessageA(m_hListView, LVM_INSERTCOLUMNA, 0, (LPARAM)&column);
}

ListView::~ListView()
{

}

void ListView::RemoveItem(uint index)
{
	if (index >= m_items.size())
		return;

	m_items.erase(m_items.begin() + index);
	//todo
}

void ListView::SelectItem(uint index)
{
	if (m_selectedItem == index)
		return;

	ListView_SetItemState(m_hListView, index, LVIS_FOCUSED | LVIS_SELECTED, 0xf);
	HandleSelectionChanged();
}

const ListViewItem* ListView::GetItem(uint index) const
{
	return (index < m_items.size()) ? &m_items[index] : nullptr;
}

void ListView::AddItemInternal(char* name, uint64 typeId, void* pItemData)
{
	ListViewItem item = { 0 };
	item.typeId = typeId;
	item.pData = pItemData;
	m_items.push_back(item);

	LVITEMA lvItem = { 0 };
	lvItem.iItem = (uint)m_items.size();
	lvItem.mask = LVIF_TEXT;
	lvItem.pszText = name;
	lvItem.cchTextMax = (uint)strlen(lvItem.pszText) + 1;
	::SendMessageA(m_hListView, LVM_INSERTITEMA, 0, (LPARAM)&lvItem);
}

void ListView::HandleSelectionChanged()
{
	m_selectedItem = (uint)::SendMessage(m_hListView, LVM_GETNEXTITEM, -1, LVNI_FOCUSED);
	if (m_selectionChangedCallback)
	{
		m_selectionChangedCallback(this, m_selectedItem, m_pUserData);
	}
}

LRESULT CALLBACK ListView::WinProc(HWND hWnd, uint msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_NOTIFY:
		ListView* pListView = (ListView*)::GetWindowLongPtr(hWnd, GWLP_USERDATA);
		if (((LPNMHDR)lParam)->code == NM_CLICK)
		{
			pListView->HandleSelectionChanged();
		}
		break;
	}

	return DefWindowProc(hWnd, msg, wParam, lParam);
}