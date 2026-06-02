#include "StdAfx.h"
#include "LogProc.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>

// Static format strings (original hardcoded strings)
//static const char* s_szLogDir = "log\\";
//static const char* s_szLogMode = "a+t";
//static const char* s_szErrorLogWrite = "error-L1 : Log write error.";

// =========================================================================
// Constructor - matches original IDA offset assignments
// Parameters: a2 = TextPosX, a3 = TextPosY, a4 = hWndParent
// =========================================================================
CLogProc::CLogProc(int nTextPosX, int nTextPosY, HWND hWndParent)
{
    // this[696] = a2 -- Text position X
    m_nTextPosX = nTextPosX;

    // vftable set by compiler

    // this[65] = 0 -- File pointer
    m_pLogFile = NULL;

    // this[691] = 0 -- File logging disabled by default
    m_bEnableFile = 0;

    // this[692] = 0
    m_nLogYear = 0;
    m_nLogMonth = 0;
    m_nLogDay = 0;

    // this[66] = 0 -- Buffer index
    m_nBufferIndex = 0;

    // this[697] = a3 -- Text position Y (actually m_bEnableFile is set above, this is Y pos)
    // Wait - looking at original: this[697] = a3, this[691] = 0 are both set.
    // this[697] is m_nTextPosY, this[691] is m_bEnableFile
    m_nTextPosY = nTextPosY;

    // this[698] = a4 -- Parent window
    m_hParentWnd = hWndParent;

    // this[699] = 0 -- Bitmap handle
    m_hBufferBitmap = NULL;

    // Create offscreen bitmap if parent window provided
    if (m_hParentWnd)
        SetParentWindowBitmap();

    // Note: m_bEnableUI is NOT set in constructor - it's set in LogInit()
    // this[690] remains uninitialized until LogInit() is called
}

// =========================================================================
// Destructor
// =========================================================================
CLogProc::~CLogProc()
{
    // Close log file if open
    LogClose();

    // Delete offscreen bitmap if created
    if (m_hBufferBitmap)
    {
        DeleteObject(m_hBufferBitmap);
        m_hBufferBitmap = NULL;
    }
}

// =========================================================================
// LogInit - Enable/Disable Logging (original: sets this[690] = a2)
// =========================================================================
void CLogProc::LogInit(int bEnable)
{

    m_bEnableUI = bEnable;

    if (bEnable)
    {
        // Clear all log buffers (25 lines, 80 bytes each = 0x50)
        for (int i = 0; i < LOG_BUFFER_COUNT; ++i)
        {
            memset(m_szLogBuffer[i], 0, LOG_BUFFER_SIZE);  // 0x50 = 80 bytes
            m_wTextLength[i] = 0;
            m_bTextColor[i] = LOG_COLOR_NORMAL;
        }
        m_nBufferIndex = 0;

        // Open log file if file output enabled
        if (m_bEnableFile)
            LogDataSet();
    }
}

// =========================================================================
// LogClose - Close Log File (original: fclose + NULL)
// =========================================================================
void CLogProc::LogClose()
{
    if (m_pLogFile)
    {
        fclose(m_pLogFile);
        m_pLogFile = NULL;
    }
}

// =========================================================================
// LogGetFileName - Return Current Log Filename (original: returns this + 4)
// =========================================================================
const char* CLogProc::LogGetFileName()
{
    return m_szLogFileName;
}

// =========================================================================
// LogDataSet - Open New Log File with Date-Based Name
// =========================================================================
void CLogProc::LogDataSet()
{
    // Get current date
    time_t currentTime;
    time(&currentTime);
    tm* today = localtime(&currentTime);

    // Store date components
    m_nLogYear = today->tm_year + 1900;
    m_nLogMonth = today->tm_mon + 1;
    m_nLogDay = today->tm_mday;

    // Build filename: log\YYMMDD.log
    char szNewFile[MAX_PATH];
    _snprintf(szNewFile, sizeof(szNewFile), "log\\%02d%02d%02d.log", m_nLogYear, m_nLogMonth, m_nLogDay);
    szNewFile[sizeof(szNewFile) - 1] = '\0';

    // Close existing file if open
    if (m_pLogFile)
    {
        fclose(m_pLogFile);
        m_pLogFile = NULL;
    }

    // Open new file in append mode
    m_pLogFile = fopen(szNewFile, "a+t");
    if (!m_pLogFile)
    {
        LogAddFunc("error-L1 : Log write error.");
        //MsgBox("error-L1 : Log write error.");
        return;
    }

    // Store filename
    strncpy_s(m_szLogFileName, sizeof(m_szLogFileName), szNewFile, _TRUNCATE);
}

// =========================================================================
// LogDateChange - Check if Date Changed, Rotate Log File
// Returns: 1 if file was rotated, 0 if no change
// =========================================================================
int CLogProc::LogDateChange()
{
    // Get current date
    time_t currentTime;
    time(&currentTime);
    tm* today = localtime(&currentTime);

    int currentYear = today->tm_year + 1900;
    int currentMonth = today->tm_mon + 1;
    int currentDay = today->tm_mday;

    // Check if date has changed
    if (currentYear <= m_nLogYear &&
        currentMonth <= m_nLogMonth &&
        currentDay <= m_nLogDay)
    {
        return 0;  // No change
    }

    // Update stored date
    m_nLogYear = currentYear;
    m_nLogMonth = currentMonth;
    m_nLogDay = currentDay;

    // Build new filename
    char szNewFile[MAX_PATH];
    _snprintf(szNewFile, sizeof(szNewFile), "%s%02d%02d%02d.log", m_nLogYear, m_nLogMonth, m_nLogDay);
    szNewFile[sizeof(szNewFile) - 1] = '\0';

    // Close old file
    if (m_pLogFile)
    {
        fclose(m_pLogFile);
        m_pLogFile = NULL;
    }

    // Open new file
    m_pLogFile = fopen(szNewFile, "a+t");
    if (!m_pLogFile)
    {
        MsgBox("error-L1 : Log write error.");
        return 0;
    }

    // Update filename
    strncpy_s(m_szLogFileName, sizeof(m_szLogFileName), szNewFile, _TRUNCATE);

    return 1;  // File rotated
}

// =========================================================================
// LogTextAdd - Add Text to Circular Buffer (original: clamps to 79, wraps at 29)
// =========================================================================
void CLogProc::LogTextAdd(BYTE color, const char* text, int len)
{
    // Clamp length to buffer size - 1 (leave room for null terminator)
    if (len > LOG_BUFFER_SIZE - 1)
        len = LOG_BUFFER_SIZE - 1;

    // Wrap buffer index if needed
    if (m_nBufferIndex >= LOG_BUFFER_COUNT)
        m_nBufferIndex = 0;

    // Get pointer to current buffer slot
    char* pSlot = m_szLogBuffer[m_nBufferIndex];

    // Clear and copy text
    memset(pSlot, 0, LOG_BUFFER_SIZE);
    if (text && len > 0)
    {
        memcpy(pSlot, text, len);
        pSlot[len] = '\0';  // Ensure null termination
    }

    // Store length and color
    m_wTextLength[m_nBufferIndex] = static_cast<WORD>(len);
    m_bTextColor[m_nBufferIndex] = static_cast<BYTE>(color);

    // Advance to next slot
    ++m_nBufferIndex;
}

// =========================================================================
// LogTextClear - Clear All Buffered Text (original: clears 25 lines)
// =========================================================================
void CLogProc::LogTextClear()
{
    for (int i = 0; i < LOG_BUFFER_COUNT; ++i)
    {
        memset(m_szLogBuffer[i], 0, LOG_BUFFER_SIZE);  // 80 bytes
        m_wTextLength[i] = 0;
        m_bTextColor[i] = LOG_COLOR_NORMAL;
    }
    m_nBufferIndex = 0;
}

// =========================================================================
// LogAddFunc - Add Normal Log Entry
// =========================================================================
void CLogProc::LogAddFunc(const char* format, ...)
{
    if (!m_bEnableUI)
        return;

    // Format message
    char szMessage[512];
    szMessage[0] = '\0';

    va_list args;
    va_start(args, format);
    _vsnprintf_s(szMessage, sizeof(szMessage), _TRUNCATE, format, args);
    va_end(args);

    // Add to circular buffer
    LogTextAdd(LOG_COLOR_NORMAL, szMessage, static_cast<int>(strlen(szMessage)));

    // Repaint parent window if available
    if (m_hParentWnd)
    {
        LogTextPaintProc(m_hParentWnd);
    }

    // Write to file if enabled
    if (m_bEnableFile && m_pLogFile)
    {
        fprintf(m_pLogFile, "%s\n", szMessage);
        fflush(m_pLogFile);  // Ensure immediate write
    }
}

// =========================================================================
// LogAddFuncColor - Add Colored Log Entry
// =========================================================================
void CLogProc::LogAddFuncColor(BYTE color, const char* format, ...)
{
    if (!m_bEnableUI)
        return;

    // Format message
    char szMessage[1024];
    szMessage[0] = '\0';

    va_list args;
    va_start(args, format);
    _vsnprintf_s(szMessage, sizeof(szMessage), _TRUNCATE, format, args);
    va_end(args);

    // Add to circular buffer with color
    LogTextAdd(color, szMessage, static_cast<int>(strlen(szMessage)));

    // Repaint parent window if available
    if (m_hParentWnd)
    {
        LogTextPaintProc(m_hParentWnd);
    }

    // Write to file if enabled (color not preserved in file)
    if (m_bEnableFile && m_pLogFile)
    {
        fprintf(m_pLogFile, "%s\n", szMessage);
        fflush(m_pLogFile);
    }
}

// =========================================================================
// LogAddHeadHexFunc - Add Hex Dump Log Entry
// =========================================================================
void CLogProc::LogAddHeadHexFunc(int type, char* data, int len)
{
    if (!m_bEnableUI)
        return;

    // Add raw data to buffer
    LogTextAdd((BYTE)type, data, len);

    // Write hex dump to file if enabled
    if (m_bEnableFile && m_pLogFile)
    {
        switch (type)
        {
        case 2:
            fprintf(m_pLogFile, "Send:[%d] : (", len);
            break;
        case 3:
            fprintf(m_pLogFile, "FDWrite:[%d] : (", len);
            break;
        case 4:
            fprintf(m_pLogFile, "Recv:[%d] : (", len);
            break;
        default:
            fprintf(m_pLogFile, "%d[%d] : (", type, len);
            break;
        }

        for (int i = 0; i < len; ++i)
            fprintf(m_pLogFile, "0x%02X ", (unsigned __int8)data[i]);

        fprintf(m_pLogFile, ")\n");
    }
}

// =========================================================================
// LogAddTimeDateFunc - Add Timestamped Log Entry
// =========================================================================
void CLogProc::LogAddTimeDateFunc(const char* format, ...)
{
    if (!m_bEnableUI)
        return;

    // Get current time
    time_t currentTime;
    time(&currentTime);
    tm* today = localtime(&currentTime);

    // Format time string: "HH:MM:SS AM/PM "
    char szTime[16];
    int hour = today->tm_hour;
    const char* szAmPm = "AM";

    if (hour >= 12)
    {
        szAmPm = "PM";
        if (hour > 12)
            hour -= 12;
    }
    if (hour == 0)
        hour = 12;

    _snprintf_s(szTime, sizeof(szTime), _TRUNCATE, "%02d:%02d:%02d %s ",
        hour, today->tm_min, today->tm_sec, szAmPm);

    // Format message
    char szMessage[1024];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(szMessage, sizeof(szMessage), _TRUNCATE, format, args);
    va_end(args);

    // Combine timestamp + message
    char szFullMessage[1040];
    _snprintf_s(szFullMessage, sizeof(szFullMessage), _TRUNCATE, "%s%s", szTime, szMessage);

    // Add to buffer
    LogTextAdd(LOG_COLOR_NORMAL, szFullMessage, static_cast<int>(strlen(szFullMessage)));

    // Write to file if enabled
    if (m_bEnableFile && m_pLogFile)
    {
        fprintf(m_pLogFile, "%s\n", szFullMessage);
        fflush(m_pLogFile);
    }
}

// =========================================================================
// LogTextPaintProc - Paint Buffered Text to Window
// =========================================================================
void CLogProc::LogTextPaintProc(HWND hWnd)
{
    if (!m_bEnableUI)
        return;

    // Clear parent window background if not using bitmap
    if (!m_hBufferBitmap)
    {
        ClearParentWindow();
    }

    HDC hdc = GetDC(hWnd);

    int total = LOG_BUFFER_COUNT;    // 25 lines
    int n = m_nBufferIndex - 1;   // Start from the most recent line

    HDC hdcSrc;
    HGDIOBJ hObj1 = NULL, hObj2 = NULL;
    HBRUSH hBrush = NULL;
    RECT Rect;

    // Logo path: create offscreen DC with white background
    if (m_hBufferBitmap)
    {
        GetClientRect(m_hParentWnd, &Rect);

        hdcSrc = CreateCompatibleDC(hdc);
        hObj1 = SelectObject(hdcSrc, (HGDIOBJ)m_hBufferBitmap);
        hBrush = CreateSolidBrush(RGB(255, 255, 255));
        hObj2 = SelectObject(hdcSrc, hBrush);
        PatBlt(hdcSrc, 0, 0, Rect.right, Rect.bottom, PATCOPY);
    }
    else
    {
        hdcSrc = hdc;  // No logo - draw directly on window DC
    }

    // Draw each log line
    while (total-- != 0)
    {
        // Set text color based on log type
        switch (m_bTextColor[n])
        {
        case 2:
            SetTextColor(hdcSrc, RGB(255, 0, 0));     // 0xFF
            break;
        case 3:
            SetTextColor(hdcSrc, RGB(0, 100, 0));      // 0x6400
            break;
        case 4:
            SetTextColor(hdcSrc, RGB(0, 0, 255));      // 0xFF0000 (blue in RGB)
            break;
        case 5:
            SetTextColor(hdcSrc, RGB(155, 0, 0));      // 0x9B
            break;
        case 6:
            SetTextColor(hdcSrc, RGB(0, 0, 100));      // 0x640000
            break;
        default:
            SetTextColor(hdcSrc, RGB(0, 0, 0));        // Black
            break;
        }

        // Draw the text if it has content
        if (strlen(m_szLogBuffer[n]) > 1)
        {
            int len = strlen(m_szLogBuffer[n]);
            TextOutA(hdcSrc, m_nTextPosX, 15 * total + m_nTextPosY, m_szLogBuffer[n], len);
        }

        // Move to previous line (older log entry)
        if (--n < 0)
            n = LOG_BUFFER_COUNT - 1;
    }

    // If logo, copy the offscreen DC to the window
    if (m_hBufferBitmap && hdcSrc)
    {
        RECT rc;
        GetClientRect(m_hParentWnd, &rc);

        BitBlt(hdc, 0, 0, Rect.right, Rect.bottom, hdcSrc, 0, 0, SRCCOPY);
        // Cleanup
        SelectObject(hdcSrc, hObj1);
        SelectObject(hdcSrc, hObj2);
        DeleteObject(hBrush);
        DeleteDC(hdcSrc);
    }
    ReleaseDC(hWnd, hdc);
}

// =========================================================================
// ClearParentWindow - Clear Parent Window Background
// =========================================================================
void CLogProc::ClearParentWindow()
{
    // Only clear if NO bitmap (bitmap handles its own white background)
    if (m_hBufferBitmap)
        return;

    HWND hWnd = m_hParentWnd;
    if (!hWnd)
        return;

    RECT Rect;
    GetClientRect(hWnd, &Rect);

    HDC hDC = GetDC(hWnd);
    HBRUSH hBrush = CreateSolidBrush(RGB(255, 255, 255));
    FillRect(hDC, &Rect, hBrush);
    DeleteObject(hBrush);
    ReleaseDC(hWnd, hDC);
}

// =========================================================================
// SetParentWindowBitmap - Create Offscreen Bitmap for Double-Buffering
// =========================================================================
void CLogProc::SetParentWindowBitmap()
{
    HWND hWnd = m_hParentWnd;
    if (!hWnd)
        return;

    HDC hDC = GetDC(hWnd);
    RECT Rect;
    GetClientRect(hWnd, &Rect);

    m_hBufferBitmap = CreateCompatibleBitmap(hDC, Rect.right, Rect.bottom);

    ReleaseDC(hWnd, hDC);
}

// =========================================================================
// MsgBox - Static Error Message Box
// =========================================================================
void CLogProc::MsgBox(const char* format, ...)
{
    char string[509];
    string[0] = 0;
    memset(&string[1], 0, 0x1FC);
    WORD v3 = 0;
    char v4 = 0;

    va_list ap;
    va_start(ap, format);
    vsprintf(string, format, ap);
    va_end(ap);

    MessageBoxA(NULL, string, "Error", MB_OK);
}

