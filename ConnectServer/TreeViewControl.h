// TreeViewControl.h
#pragma once

#ifndef __TREEVIEWCONTROL_H__
#define __TREEVIEWCONTROL_H__

#include <Windows.h>
#include <CommCtrl.h>

#define MAX_TREE_ITEMS 512

class CTreeViewControl
{
public:
	CTreeViewControl(int nMaxItems = MAX_TREE_ITEMS);
	virtual ~CTreeViewControl();

	void Start(HWND hTreeView, HWND hParentWnd);

	void AddImagetoImageList(HICON hIcon);

	void InsertServerNode(int nImage, int nSelectedImage, char* pszText);
	void InsertSubServerNode(int nImage, int nSelectedImage, char* pszText);

	void TreeNotify(unsigned int a2, int a3);

	static LRESULT CALLBACK TreeViewSubProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

public:

	// +0x04
	HWND m_hTreeView;

	// +0x08
	HWND m_hParentWnd;

	// +0x0C
	HIMAGELIST m_hImageList;

	// +0x10
	HTREEITEM* m_pItemArray;

	// +0x14
	HTREEITEM* m_pCurrentItem;

	// +0x18
	HTREEITEM m_hInsertParent;

	// +0x1C
	HTREEITEM m_hInsertAfter;

	// +0x20
	UINT m_InsertMask;

	// +0x24
	TV_INSERTSTRUCTA m_InsertStruct;

	// +0x4C
	int m_nMaxItems;

	// +0x50
	int m_nItemCount;

public:
	
};
extern WNDPROC g_OldTreeViewProc;
#endif