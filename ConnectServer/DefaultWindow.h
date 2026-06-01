// DefaultWindow.h
#pragma once

#ifndef __DEFAULTWINDOW_H__
#define __DEFAULTWINDOW_H__

#include <Windows.h>

class CDefaultWindow
{
public:
	CDefaultWindow();
	virtual ~CDefaultWindow();

public:
	void mfn_CreateWindow(HINSTANCE hInst, HWND hWndParent, int X, int Y, int nWidth, int nHeight, DWORD dwStyle);
	void mfn_CenterWindow();
	void mfn_ClearWindow();

public:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
public:

	// +0x04
	HINSTANCE m_hInstance;
	// +0x08
	HWND m_hWnd;
	// +0x0C
	char m_szClassName[32];
};

extern CDefaultWindow CDW;

#endif