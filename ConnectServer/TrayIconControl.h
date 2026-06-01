#pragma once

#ifndef __TRAYICONCONTROL_H__
#define __TRAYICONCONTROL_H__
#include <ShellAPI.h>

// Tray icon callback message (WM_USER + 100 = 0x8000 + 100 = 32868)
#define WM_TRAYICON_CALLBACK (WM_USER + 100)

// NOTIFYICONDATA size for Windows 2000/XP compatibility (88 bytes)
#define NOTIFYICONDATA_SIZE_V1 88

class CTrayIconControl
{
public:
    // Constructor / Destructor
    CTrayIconControl();
    virtual ~CTrayIconControl();

    // Initialization
    void Start(HINSTANCE hInstance, HWND hWndParent, UINT nIconResID, const char* szToolTip);

    // Icon management
    void SetTrayIcon(UINT nIconResID, const char* szToolTip);
    void DeleteTrayIcon();

    // Message handler - call from main window's WndProc
    void TrayNotify(HWND hWnd, UINT uMsg, LPARAM lParam);

    // Accessors
    HWND GetParentWindow() const { return m_hParentWnd; }
    HINSTANCE GetInstance() const { return m_hInstance; }
    UINT GetCallbackMessage() const { return m_uCallbackMsg; }
    UINT GetIconID() const { return m_uIconID; }

protected:
    // Member variables (layout matches original binary offsets)

    // [0x00] Virtual function table pointer (compiler-managed)

    // [0x04] Module instance handle (HINSTANCE)
    HINSTANCE m_hInstance;

    // [0x08] Parent window handle (for message routing)
    HWND m_hParentWnd;

    // [0x0C] NOTIFYICONDATA structure (88 bytes for Win2000/XP)
    // This is embedded directly in the class at offset 12
    union {
        NOTIFYICONDATAA nid;
        BYTE nidBuffer[NOTIFYICONDATA_SIZE_V1];
    } m_nid;

    // [0x64] Additional state (if needed for future expansion)
    // Original has padding here

    // Cached values for quick access
    UINT m_uIconID;              // Current icon resource ID
    UINT m_uCallbackMsg;         // Callback message (WM_TRAYICON_CALLBACK)
    UINT m_uFlags;               // NIF_* flags for Shell_NotifyIcon
    UINT m_uVersion;             // NOTIFYICON_VERSION for extended features

private:
    // Helper: Initialize NOTIFYICONDATA structure
    void InitNotifyIconData();

    // Helper: Update tray icon via Shell_NotifyIcon
    BOOL UpdateTrayIcon(DWORD dwMessage);
};
extern CTrayIconControl CTrayCtl;

#endif // __TRAYICONCONTROL_H__