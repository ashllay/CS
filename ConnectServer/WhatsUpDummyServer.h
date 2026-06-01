#pragma once

#ifndef __WHATSUPDUMMYSERVER_H__
#define __WHATSUPDUMMYSERVER_H__

#include <winsock2.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

// Async socket message for dummy server (WM_USER + 1039 = 0x8000 + 1039 = 33863 = 0x40F)
#define WM_WHATSUP_EVENT (WM_USER + 1039)

class CWhatsUpDummyServer
{
public:
    // Constructor / Destructor
    CWhatsUpDummyServer();
    virtual ~CWhatsUpDummyServer();

    // Initialization
    BOOL Start(HWND hWnd, unsigned short nPort);

    // Static window procedure for async socket messages
    static LRESULT CALLBACK ParentWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

    // Accessors
    SOCKET GetSocket() const { return m_hSocket; }
    HWND GetNotifyWindow() const { return m_hNotifyWnd; }
    unsigned short GetPort() const { return m_nPort; }

    // Static members
    static WNDPROC m_lpOldProc;  // Original window proc for subclassing

protected:
    // Member variables (functional equivalent of original layout)

    // [0x00] Listening socket handle (INVALID_SOCKET = -1 when unused)
    SOCKET m_hSocket;

    // [0x04] Reserved/padding (original offset)
    DWORD m_dwReserved;

    // [0x08] Notify window handle (for WSAAsyncSelect)
    HWND m_hNotifyWnd;

    // [0x0C] Listening port
    unsigned short m_nPort;

private:
    // Helper: Process received "What's Up" probe
    void ProcessProbe(SOCKET hClient);
};

#endif // __WHATSUPDUMMYSERVER_H__