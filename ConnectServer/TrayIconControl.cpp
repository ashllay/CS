#include "stdafx.h"
#include "TrayIconControl.h"

// =========================================================================
// Constructor
// =========================================================================
CTrayIconControl::CTrayIconControl()
    : m_hInstance(NULL)
    , m_hParentWnd(NULL)
    , m_uIconID(0)
    , m_uCallbackMsg(WM_TRAYICON_CALLBACK)
    , m_uFlags(NIF_ICON | NIF_MESSAGE | NIF_TIP)  // 7 = 0x01 | 0x02 | 0x04
    , m_uVersion(0)
{
    // Zero-initialize the embedded NOTIFYICONDATA buffer (offset 12, 88 bytes)
    ZeroMemory(&m_nid, 88);

    // Initialize common NOTIFYICONDATA fields
    InitNotifyIconData();
}

// =========================================================================
// Destructor
// =========================================================================
CTrayIconControl::~CTrayIconControl()
{
    // Remove tray icon if it exists
    DeleteTrayIcon();
}

// =========================================================================
// InitNotifyIconData - Helper: Setup NOTIFYICONDATA Structure
// =========================================================================
void CTrayIconControl::InitNotifyIconData()
{
    // Ensure structure size is correct for target OS
    m_nid.nid.cbSize = NOTIFYICONDATA_SIZE_V1;  // 88 bytes for Win2000/XP

    // Set flags: icon + message + tooltip
    m_nid.nid.uFlags = m_uFlags;  // 7 = NIF_ICON | NIF_MESSAGE | NIF_TIP

    // Set callback message for mouse events
    m_nid.nid.uCallbackMessage = m_uCallbackMsg;  // 32868 = WM_USER + 100

    // Note: hWnd, uID, hIcon, szTip are set by Start/SetTrayIcon
}

// =========================================================================
// UpdateTrayIcon - Helper: Call Shell_NotifyIcon with Proper Params
// =========================================================================
BOOL CTrayIconControl::UpdateTrayIcon(DWORD dwMessage)
{
    if (!m_hParentWnd)
        return FALSE;

    // Ensure hWnd is set in NOTIFYICONDATA
    m_nid.nid.hWnd = m_hParentWnd;
    m_nid.nid.uID = 1;  // Fixed ID for single icon

    return Shell_NotifyIconA(dwMessage, &m_nid.nid) ? TRUE : FALSE;
}

// =========================================================================
// Start - Initialize and Add Tray Icon
// =========================================================================
void CTrayIconControl::Start(HINSTANCE hInstance, HWND hWndParent, UINT nIconResID, const char* szToolTip)
{
    // Store handles
    m_hInstance = hInstance;
    m_hParentWnd = hWndParent;

    // Load icon from resources
    HICON hIcon = LoadIconA(hInstance, MAKEINTRESOURCEA(nIconResID));
    if (!hIcon)
        return;  // Could log error here

    // Set icon in NOTIFYICONDATA
    m_nid.nid.hIcon = hIcon;
    m_uIconID = nIconResID;

    // Copy tooltip text (max 64 chars for NOTIFYICONDATAA.szTip)
    if (szToolTip)
    {
        strncpy_s(m_nid.nid.szTip, sizeof(m_nid.nid.szTip), szToolTip, _TRUNCATE);
    }

    // Initialize structure fields
    InitNotifyIconData();

    // Add icon to system tray (NIM_ADD = 0)
    UpdateTrayIcon(NIM_ADD);

    // Optional: Set version for extended features (Vista+)
    // m_nid.nid.uVersion = NOTIFYICON_VERSION;
    // Shell_NotifyIcon(NIM_SETVERSION, &m_nid.nid);
}

// =========================================================================
// SetTrayIcon - Update Existing Tray Icon
// =========================================================================
void CTrayIconControl::SetTrayIcon(UINT nIconResID, const char* szToolTip)
{
    if (!m_hParentWnd)
        return;

    // Load new icon
    HICON hIcon = LoadIconA(m_hInstance, MAKEINTRESOURCEA(nIconResID));
    if (!hIcon)
        return;

    // Update NOTIFYICONDATA
    m_nid.nid.hIcon = hIcon;
    m_uIconID = nIconResID;

    if (szToolTip)
    {
        strncpy_s(m_nid.nid.szTip, sizeof(m_nid.nid.szTip), szToolTip, _TRUNCATE);
    }

    InitNotifyIconData();

    // Modify existing icon (NIM_MODIFY = 1)
    UpdateTrayIcon(NIM_MODIFY);
}

// =========================================================================
// DeleteTrayIcon - Remove Tray Icon
// =========================================================================
void CTrayIconControl::DeleteTrayIcon()
{
    if (!m_hParentWnd)
        return;

    // Ensure structure is properly configured for deletion
    m_nid.nid.cbSize = NOTIFYICONDATA_SIZE_V1;
    m_nid.nid.hWnd = m_hParentWnd;
    m_nid.nid.uID = 1;

    // Delete icon from system tray (NIM_DELETE = 2)
    Shell_NotifyIconA(NIM_DELETE, &m_nid.nid);

    // Clean up icon handle if needed (LoadIcon returns system-owned handle)
    // Note: LoadIcon icons are managed by system, don't call DestroyIcon
}

// =========================================================================
// TrayNotify - Handle Mouse Events from Tray Icon
// =========================================================================
void CTrayIconControl::TrayNotify(HWND hWnd, UINT uMsg, LPARAM lParam)
{
    // lParam contains the mouse message (WM_LBUTTONDOWN, etc.)
    switch (lParam)
    {
    case WM_LBUTTONDBLCLK:  // 515 = Double-click
    {
        // Restore and focus parent window
        ShowWindow(m_hParentWnd, SW_RESTORE);  // 5 = SW_SHOW
        ShowWindow(m_hParentWnd, SW_SHOW);      // 1 = SW_SHOWNORMAL (redundant but matches original)
        SetForegroundWindow(m_hParentWnd);
        SetFocus(m_hParentWnd);
    }
    break;

    case WM_RBUTTONUP:  // 516 = Right-click release
    {
        // Load context menu from resources (IDR_TRAY_MENU = 137 = 0x89)
        HMENU hMenu = LoadMenuA(m_hInstance, MAKEINTRESOURCEA(137));
        if (hMenu)
        {
            HMENU hSubMenu = GetSubMenu(hMenu, 0);
            if (hSubMenu)
            {
                // Get cursor position for menu placement
                POINT pt;
                GetCursorPos(&pt);

                // CRITICAL: Set foreground window before TrackPopupMenu
                // Otherwise menu won't dismiss properly on click-away
                SetForegroundWindow(hWnd);

                // Show context menu (TPM_RIGHTBUTTON = 2)
                TrackPopupMenu(hSubMenu, TPM_RIGHTBUTTON | TPM_LEFTALIGN,
                    pt.x, pt.y, 0, hWnd, NULL);

                // Post dummy message to fix menu dismissal bug on XP/Vista
                PostMessage(hWnd, WM_NULL, 0, 0);
            }
            DestroyMenu(hMenu);
        }
    }
    break;

    // Optional: Handle other messages
    case WM_LBUTTONDOWN:   // Single click
    case WM_RBUTTONDOWN:   // Right click down
    case WM_MOUSEMOVE:     // Hover (requires NIF_SHOWTIP)
        // Add custom handling if needed
        break;
    }
}