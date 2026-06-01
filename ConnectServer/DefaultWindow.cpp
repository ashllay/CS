// DefaultWindow.cpp
#include "StdAfx.h"
#include "DefaultWindow.h"
#include "LogProc.h"

char WindowName[] = "접속서버 인터페이스";

CDefaultWindow::CDefaultWindow()
{
	strcpy(m_szClassName, "DefaultWndClass");
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
	WndClass.hCursor = LoadCursorA(0, (LPCSTR)IDC_ARROW);
	WndClass.hIcon = LoadIconA(0, (LPCSTR)IDI_APPLICATION);
	WndClass.hInstance = hInst;
	WndClass.lpfnWndProc = this->WndProc;
	WndClass.lpszClassName = m_szClassName;
	WndClass.lpszMenuName = 0;
	WndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	RegisterClassA(&WndClass);

	Window = CreateWindowExA(
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
	// Forward logging-related messages to CLogProc
	switch (Msg)
	{
	case WM_CREATE:  // 1
	{
		// Initialize logging system
		// Original: CLogProc::LogInit((CLogProc*)g_LogProc, 1)
		g_LogProc.LogInit(TRUE);
		return 0;
	}

	case WM_DESTROY:  // 2
	{
		// Post quit message to end message loop
		PostQuitMessage(0);
		return 0;
	}

	case WM_SIZE:  // 5
	case WM_PAINT:  // 15
	{
		// Paint log text to window
		// Original: CLogProc::LogTextPaintProc((CLogProc*)g_LogProc, hWnd)
		g_LogProc.LogTextPaintProc(hWnd);
		// Fall through to DefWindowProc for default painting
		break;
	}

	case WM_LBUTTONDOWN:  // 0x201 = 513
	case WM_LBUTTONDBLCLK:  // 0x203 = 515
	case WM_NCHITTEST:  // 0x84 = 132 (not in original but common)
	{
		// Swallow mouse messages to prevent default behavior
		// Original returns 0 for these
		return FALSE;
	}
	}

	// Default message handling
	return DefWindowProcA(hWnd, Msg, wParam, lParam);
}