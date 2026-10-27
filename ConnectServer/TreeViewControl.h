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
	void TreeNotify(WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK TreeViewSubProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

public:
	HWND m_hTreeView;					// +0x04
	HWND m_hParentWnd;					// +0x08
	HIMAGELIST m_hImageList;			// +0x0C
	HTREEITEM* m_pItemArray;			// +0x10
	HTREEITEM* m_pCurrentItem;			// +0x14
	// Original layout: TV_INSERTSTRUCTA at +0x18, m_nMaxItems +0x4C,
	// m_nItemCount +0x50 (VS2008 SDK, TV_INSERTSTRUCTA size 0x34).
	// The current SDK's TV_INSERTSTRUCTA is larger, so the trailing member
	// offsets shift accordingly; only this file accesses them.
	TV_INSERTSTRUCTA m_InsertStruct;	// +0x18
	int m_nMaxItems;					// +0x5C in this build
	int m_nItemCount;					// +0x60 in this build
public:
	static WNDPROC g_OldTreeViewProc;
};

#endif