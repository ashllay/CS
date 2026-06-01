#include "StdAfx.h"
#include "LogToListBox.h"

// =========================================================================
// Constructor
// =========================================================================
CLogToListBox::CLogToListBox(int bWithDate, HWND hListBox)
    : m_hListBox(hListBox)
    , m_bWithDate(bWithDate)
{
    // Initialize buffers
    ZeroMemory(m_szFormatBuffer, sizeof(m_szFormatBuffer));
    ZeroMemory(m_szOutputBuffer, sizeof(m_szOutputBuffer));

    // Initialize critical section for thread-safe logging
    InitializeCriticalSection(&m_cs);
}

// =========================================================================
// Destructor
// =========================================================================
CLogToListBox::~CLogToListBox()
{
    // Clean up synchronization object
    DeleteCriticalSection(&m_cs);
}

// =========================================================================
// SetListBox - Update Target ListBox Control
// =========================================================================
void CLogToListBox::SetListBox(HWND hListBox)
{
    m_hListBox = hListBox;
}

// =========================================================================
// SetWithDate - Enable/Disable Timestamp Prefix
// =========================================================================
void CLogToListBox::SetWithDate(int bWithDate)
{
    m_bWithDate = (bWithDate != 0);
}

// =========================================================================
// FormatOutput - Helper: Build Output String with Optional Timestamp
// =========================================================================
void CLogToListBox::FormatOutput(const char* szMessage)
{
    if (!szMessage)
        return;

    if (m_bWithDate)
    {
        // Get current local time
        SYSTEMTIME st;
        GetLocalTime(&st);

        // Format: "YYYY-MM-DD HH:MM:SS     message"
        _snprintf_s(m_szOutputBuffer, sizeof(m_szOutputBuffer), _TRUNCATE,
            "%04d-%02d-%02d %02d:%02d:%02d     %s",
            st.wYear, st.wMonth, st.wDay,
            st.wHour, st.wMinute, st.wSecond,
            szMessage);
    }
    else
    {
        // Format: "message\n"
        _snprintf_s(m_szOutputBuffer, sizeof(m_szOutputBuffer), _TRUNCATE,
            "%s\n", szMessage);
    }
}

// =========================================================================
// AutoScroll - Helper: Scroll ListBox to Show Latest Entry
// =========================================================================
void CLogToListBox::AutoScroll()
{
    if (!m_hListBox || !IsWindow(m_hListBox))
        return;

    // Get item count (LB_GETCOUNT = 0x18B)
    int nCount = static_cast<int>(SendMessageA(m_hListBox, LB_GETCOUNT, 0, 0));

    if (nCount > 0)
    {
        // Set top index to last visible item (LB_SETTOPINDEX = 0x186)
        // This scrolls to show the newest entry
        SendMessageA(m_hListBox, LB_SETTOPINDEX, nCount - 1, 0);
    }
}

// =========================================================================
// Output - Thread-Safe Logging to ListBox
// =========================================================================
void CLogToListBox::Output(const char* format, ...)
{
    if (!m_hListBox || !IsWindow(m_hListBox) || !format)
        return;

    // Format message using variable arguments
    va_list args;
    va_start(args, format);
    _vsnprintf_s(m_szFormatBuffer, sizeof(m_szFormatBuffer), _TRUNCATE, format, args);
    va_end(args);

    // Thread-safe section: format and send to UI
    EnterCriticalSection(&m_cs);

    // Build output string with optional timestamp
    FormatOutput(m_szFormatBuffer);

    // Add string to listbox (LB_ADDSTRING = 0x180)
    SendMessageA(m_hListBox, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(m_szOutputBuffer));

    // Auto-scroll to bottom
    AutoScroll();

    LeaveCriticalSection(&m_cs);

    // Trigger error log alarm (UI alert for new entries)
    // Note: Called outside critical section to avoid potential deadlock
    //StartErrorLogAlarm();
}

// =========================================================================
// ResetContents - Clear All ListBox Items
// =========================================================================
void CLogToListBox::ResetContents()
{
    if (m_hListBox && IsWindow(m_hListBox))
    {
        // LB_RESETCONTENT = 0x184
        SendMessageA(m_hListBox, LB_RESETCONTENT, 0, 0);
    }
}