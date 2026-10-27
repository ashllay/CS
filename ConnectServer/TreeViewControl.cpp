// TreeViewControl.cpp
#include "StdAfx.h"
#include "TreeViewControl.h"

#include "CS.h"

WNDPROC CTreeViewControl::g_OldTreeViewProc = NULL;


CTreeViewControl::CTreeViewControl(int nMaxItems)
{
	INITCOMMONCONTROLSEX icce;

	icce.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icce.dwICC = ICC_TREEVIEW_CLASSES;

	InitCommonControlsEx(&icce);

	m_hTreeView = NULL;
	m_hParentWnd = NULL;

	m_hImageList = ImageList_Create(16, 16, ILC_COLOR8, 0, 1);

	m_pItemArray = new HTREEITEM[nMaxItems];

	for (int i = 0; i < 2048; i += 4)
	{
		*(DWORD*)((DWORD)m_pItemArray + i) = 0;
	}

	m_nMaxItems = nMaxItems;
	m_nItemCount = 0;

	ZeroMemory(&m_InsertStruct, sizeof(m_InsertStruct));

	// Original constructor (0x40D390) sets hInsertAfter = TVI_LAST and the
	// item mask once; the insert helpers only overwrite the per-item fields.
	m_InsertStruct.hInsertAfter = TVI_LAST;
	m_InsertStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
}

CTreeViewControl::~CTreeViewControl()
{
	delete[] m_pItemArray;

	ImageList_Destroy(m_hImageList);
}

void CTreeViewControl::Start(HWND hTreeView, HWND hParentWnd)
{
	m_hTreeView = hTreeView;
	m_hParentWnd = hParentWnd;
}

void CTreeViewControl::AddImagetoImageList(HICON hIcon)
{
	ImageList_ReplaceIcon(m_hImageList, -1, hIcon);

	SendMessageA(m_hTreeView, TVM_SETIMAGELIST, 0, (LPARAM)m_hImageList);
}

void CTreeViewControl::InsertServerNode(int nImage, int nSelectedImage, char* pszText)
{
	if (m_nItemCount < m_nMaxItems)
	{
		m_InsertStruct.item.iSelectedImage = nSelectedImage;
		m_InsertStruct.item.iImage = nImage;
		m_InsertStruct.hParent = TVI_ROOT;
		m_InsertStruct.hInsertAfter = TVI_LAST;
		m_InsertStruct.item.pszText = pszText;
		m_InsertStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

		m_pItemArray[m_nItemCount] = (HTREEITEM)SendMessageA(m_hTreeView, TVM_INSERTITEMA, 0, (LPARAM)&m_InsertStruct);
		m_pCurrentItem = &m_pItemArray[m_nItemCount];
		m_nItemCount++;
	}
}

void CTreeViewControl::InsertSubServerNode(int nImage, int nSelectedImage, char* pszText)
{
	if (m_nItemCount < m_nMaxItems)
	{
		m_InsertStruct.hParent = *m_pCurrentItem;
		m_InsertStruct.hInsertAfter = TVI_LAST;
		m_InsertStruct.item.iSelectedImage = nSelectedImage;
		m_InsertStruct.item.iImage = nImage;
		m_InsertStruct.item.pszText = pszText;
		m_InsertStruct.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

		m_pItemArray[m_nItemCount] = (HTREEITEM)SendMessageA(m_hTreeView, TVM_INSERTITEMA, 0, (LPARAM)&m_InsertStruct);
		m_nItemCount++;
	}
}

void CTreeViewControl::TreeNotify(WPARAM wParam, LPARAM lParam)
{
}

LRESULT CALLBACK CTreeViewControl::TreeViewSubProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_LBUTTONDBLCLK)
	{
		HTREEITEM hItem =
			(HTREEITEM)SendMessageA(hWnd, TVM_GETNEXTITEM, TVGN_CARET, 0);

		if (hItem)
		{
			TVITEMA tvi;
			char szText[64];

			ZeroMemory(&tvi, sizeof(tvi));

			// Original 0x40D5EE requests mask 65 = TVIF_TEXT | TVIF_CHILDREN.
			// TVIF_CHILDREN is what makes TVM_GETITEM fill cChildren, so the
			// "is this a leaf (server) node" test below actually works.
			tvi.mask = TVIF_TEXT | TVIF_CHILDREN;
			tvi.hItem = hItem;
			tvi.pszText = szText;
			tvi.cchTextMax = sizeof(szText);

			SendMessageA(hWnd, TVM_GETITEMA, 0, (LPARAM)&tvi);

			// Only leaf nodes are servers; group nodes have children and are
			// not toggleable. (The original also formats an unused string at
			// 0x40D62D; it has no observable effect and is omitted.)
			if (tvi.cChildren == 0)
			{
				InverseGameServer_ShowHide(hWnd, hItem, szText);
			}
		}
	}

	return CallWindowProcA(g_OldTreeViewProc, hWnd, Msg, wParam, lParam);
}