// TreeViewControl.cpp
#include "StdAfx.h"
#include "TreeViewControl.h"

WNDPROC /*CTreeViewControl::*/g_OldTreeViewProc = NULL;

//extern void __cdecl InverseGameServer_ShowHide(HWND hWnd,HTREEITEM* hItem,char* pszText);

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

	m_hInsertParent = TVI_ROOT;
	m_hInsertAfter = TVI_LAST;

	m_nMaxItems = nMaxItems;
	m_nItemCount = 0;

	ZeroMemory(&m_InsertStruct, sizeof(m_InsertStruct));

	m_InsertMask = 0xFFFF0000;
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
	HWND hWnd;
	int nIndex;

	if (m_nItemCount < m_nMaxItems)
	{
		m_InsertStruct.item.iSelectedImage = nSelectedImage;
		hWnd = m_hTreeView;
		m_InsertStruct.item.iImage = nImage;
		m_hInsertParent = TVI_ROOT;
		m_InsertStruct.item.pszText = pszText;
		*(HTREEITEM*)((DWORD)m_pItemArray + (4 * m_nItemCount)) = (HTREEITEM)SendMessageA(hWnd, TVM_INSERTITEMA, 0, (LPARAM)&m_hInsertParent);
		nIndex = m_nItemCount;
		m_pCurrentItem = (HTREEITEM*)((DWORD)m_pItemArray + (4 * nIndex));
		m_nItemCount = nIndex + 1;
	}
}

void CTreeViewControl::InsertSubServerNode(int nImage, int nSelectedImage, char* pszText)
{
	HWND hWnd;

	if (m_nItemCount < m_nMaxItems)
	{
		m_hInsertParent = *m_pCurrentItem;
		m_InsertStruct.item.iSelectedImage = nSelectedImage;
		hWnd = m_hTreeView;
		m_InsertStruct.item.iImage = nImage;
		m_InsertStruct.item.pszText = pszText;
		*(HTREEITEM*)((DWORD)m_pItemArray + (4 * m_nItemCount++)) = (HTREEITEM)SendMessageA(hWnd, TVM_INSERTITEMA, 0, (LPARAM)&m_hInsertParent);
	}
}

void CTreeViewControl::TreeNotify(unsigned int a2, int a3)
{
}

LRESULT CALLBACK CTreeViewControl::TreeViewSubProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	HTREEITEM hItem;
	LPARAM ItemData[8];
	int ItemParam;
	char szText[64];
	char szBuffer[64];
	const char* pszType;

	if (Msg == WM_LBUTTONDBLCLK)
	{
		hItem = (HTREEITEM)SendMessageA(hWnd, TVM_GETNEXTITEM, TVGN_CARET, 0);

		if (hItem)
		{
			ItemData[0] = TVIF_TEXT | TVIF_PARAM;
			ItemData[1] = (LPARAM)hItem;
			ItemData[4] = (LPARAM)szText;
			ItemData[5] = 64;

			SendMessageA(hWnd, TVM_GETITEMA, 0, (LPARAM)ItemData);

			ItemParam = (int)ItemData[2];

			pszType = "부모";

			if (!ItemParam)
			{
				pszType = "자식";
			}

			/*wsprintfA(szBuffer, "%s 선택됨 : %s", szText, pszType);

			if (!ItemParam)
			{
				InverseGameServer_ShowHide(hWnd, &hItem, szText);
			}*/
		}
	}
	return CallWindowProcA(g_OldTreeViewProc, hWnd, Msg, wParam, lParam);
}