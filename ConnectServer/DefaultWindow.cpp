#include "StdAfx.h"
#include "DefaultWindow.h"
#include "LogProc.h"

char WindowName[] = "접속서버 인터페이스";

CDefaultWindow::CDefaultWindow()
{
	strcpy(m_szClassName, "DEFAULT_WNDCLASS");
}

CDefaultWindow::~CDefaultWindow()
{
}

void CDefaultWindow::mfn_CreateWindow(HINSTANCE hInst, HWND hWndParent, int X, int Y, int nWidth, int nHeight, DWORD dwStyle)
{
	HWND Window;
	WNDCLASSA WndClass;

	WndClass.cbClsExtra = 0;

	m_hInstance = hInst;

	WndClass.cbWndExtra = 0;
	WndClass.hbrBackground = (HBRUSH)GetStockObject(0);
	WndClass.hCursor = LoadCursor(0, (LPCSTR)IDC_ARROW);
	WndClass.hIcon = LoadIcon(0, (LPCSTR)IDI_APPLICATION);
	WndClass.hInstance = hInst;
	WndClass.lpfnWndProc = CDefaultWindow::WndProc;
	WndClass.lpszClassName = m_szClassName;
	WndClass.lpszMenuName = 0;
	WndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	RegisterClass(&WndClass);

	Window = CreateWindowEx(
		0,
		WndClass.lpszClassName,
		WindowName,
		dwStyle,
		X,
		Y,
		nWidth,
		nHeight,
		hWndParent,
		0,
		m_hInstance,
		0);

	m_hWnd = Window;

	// Original uses ShowWindow with nCmdShow = 5 (SW_SHOW)
	ShowWindow(Window, SW_SHOW);
}

void CDefaultWindow::mfn_CenterWindow()
{
	int ScreenWidth;
	int ScreenHeight;
	RECT Rect;

	ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
	ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
	GetWindowRect(m_hWnd, &Rect);

	MoveWindow(m_hWnd, (ScreenWidth - Rect.right) / 2, (ScreenHeight - Rect.bottom) / 2, Rect.right, Rect.bottom, TRUE);
}

void CDefaultWindow::mfn_ClearWindow()
{
	HDC hDC;
	HBRUSH hBrush;
	RECT Rect;

	GetClientRect(m_hWnd, &Rect);
	hDC = GetDC(m_hWnd);
	hBrush = CreateSolidBrush(RGB(255, 255, 255));
	FillRect(hDC, &Rect, hBrush);
	DeleteObject(hBrush);

	ReleaseDC(m_hWnd, hDC);
}

LRESULT CALLBACK CDefaultWindow::WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	// Match original EXACT logic from IDA
	if (Msg > 0xF)
	{
		// Messages > 15
		if (Msg != 20 && Msg != 513 && Msg != 515)
			return DefWindowProcA(hWnd, Msg, wParam, lParam);
		return 0;  // WM_ERASEBKGND (20), WM_LBUTTONDOWN (513), WM_LBUTTONDBLCLK (515)
	}

	// Messages 0-15
	if (Msg != 15)  // Not WM_PAINT
	{
		if (Msg == 1)  // WM_CREATE
		{
			g_LogProc.LogInit(TRUE);
			return 0;
		}

		if (Msg != 2)  // Not WM_DESTROY
		{
			if (Msg != 5)  // Not WM_SIZE
				return DefWindowProcA(hWnd, Msg, wParam, lParam);
			// WM_SIZE (5) falls through to LABEL_10
		}
		else  // WM_DESTROY (2)
		{
			PostQuitMessage(0);
			return 0;
		}
	}

	// LABEL_10: WM_PAINT (15) and WM_SIZE (5) end up here
	g_LogProc.LogTextPaintProc(hWnd);
	return DefWindowProcA(hWnd, Msg, wParam, lParam);
}