#include "StdAfx.h"
#include "LogProc.h"
#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>

// Static format strings (original hardcoded strings)
static const char* s_szLogDir = "log\\";
static const char* s_szLogMode = "a+t";
static const char* s_szErrorLogWrite = "error-L1 : Log write error.";

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
            memset(m_szLogBuffer[i], 0, 0x50);  // 0x50 = 80 bytes
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
    time_t timeptr;
    time(&timeptr);
    tm* v2 = localtime(&timeptr);

    int v3 = v2->tm_year + 1900;
    v2->tm_year = v3;
    m_nLogYear = v3;

    int v4 = v2->tm_mon + 1;
    m_nLogMonth = v4;

    int tm_mday = v2->tm_mday;
    m_nLogDay = tm_mday;

    // Build filename: log\YYMMDD.log
    char file[52];
    wsprintfA(file, "log\\%02d%02d%02d.log", v3, v4, tm_mday);

    // Close existing file if open
    if (m_pLogFile)
    {
        fclose(m_pLogFile);
        m_pLogFile = NULL;
    }

    // Open new file in append mode
    m_pLogFile = fopen(file, s_szLogMode);
    if (!m_pLogFile)
    {
        LogAddFunc(s_szErrorLogWrite);
        return;
    }

    // Store filename
    strcpy(m_szLogFileName, file);
}

// =========================================================================
// LogDateChange - Check if Date Changed, Rotate Log File
// Returns: 1 if file was rotated, 0 if no change
// =========================================================================
int CLogProc::LogDateChange()
{
    // Get current date
    time_t timeptr;
    time(&timeptr);
    tm* v2 = localtime(&timeptr);

    int v3 = v2->tm_year + 1900;
    int v4 = v2->tm_mon + 1;
    v2->tm_year = v3;
    v2->tm_mon = v4;

    // Check if date has changed
    if (v3 <= m_nLogYear && v4 <= m_nLogMonth && v2->tm_mday <= m_nLogDay)
        return 0;  // No change

    // Update stored date
    m_nLogYear = v3;
    int tm_mon = v2->tm_mon;
    m_nLogMonth = tm_mon;
    int tm_mday = v2->tm_mday;
    m_nLogDay = tm_mday;

    // Build new filename
    char file[20];
    wsprintfA(file, "log\\%02d%02d%02d.log", m_nLogYear, tm_mon, tm_mday);

    // Close old file
    if (m_pLogFile)
    {
        fclose(m_pLogFile);
        m_pLogFile = NULL;
    }

    // Open new file
    m_pLogFile = fopen(file, s_szLogMode);
    if (!m_pLogFile)
    {
        MsgBox(s_szErrorLogWrite);
        return 0;
    }

    // Update filename
    strcpy(m_szLogFileName, file);

    return 1;  // File rotated
}

// =========================================================================
// LogTextAdd - Add Text to Circular Buffer (original: clamps to 79, wraps at 29)
// =========================================================================
void CLogProc::LogTextAdd(BYTE color, const char* text, int len)
{
    unsigned int v4 = len;
    if (len > 79)  // 0x4F = 79
        v4 = 79;

    if (m_nBufferIndex > 29)
        m_nBufferIndex = 0;

    // Clear and copy text to current buffer slot
    char* pSlot = m_szLogBuffer[m_nBufferIndex];
    pSlot[0] = 0;
    memcpy(pSlot, text, v4);
    pSlot[v4 + 1] = 0;   // Null after the text
    pSlot[v4] = 0;       // Null at end of text
    m_wTextLength[m_nBufferIndex] = (WORD)v4;
    m_bTextColor[m_nBufferIndex] = color;

    ++m_nBufferIndex;
}

// =========================================================================
// LogTextClear - Clear All Buffered Text (original: clears 25 lines)
// =========================================================================
void CLogProc::LogTextClear()
{
    for (int i = 0; i < LOG_BUFFER_COUNT; ++i)
    {
        memset(m_szLogBuffer[i], 0, 0x50);  // 80 bytes
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
    char string[509];
    string[0] = 0;
    memset(&string[1], 0, 0x1FC);  // 508 bytes
    WORD v4 = 0;
    char v5 = 0;

    va_list ap;
    va_start(ap, format);
    vsprintf(string, format, ap);
    va_end(ap);

    // Add to circular buffer
    LogTextAdd(LOG_COLOR_NORMAL, string, (int)strlen(string));

    // Repaint parent window if available
    if (m_hParentWnd)
        LogTextPaintProc(m_hParentWnd);

    // Write to file if enabled
    if (m_bEnableFile && m_pLogFile)
        fprintf(m_pLogFile, "%s\n", string);
}

// =========================================================================
// LogAddFuncColor - Add Colored Log Entry
// =========================================================================
void CLogProc::LogAddFuncColor(BYTE color, const char* format, ...)
{
    if (!m_bEnableUI)
        return;

    // Format message
    char string[1021];
    string[0] = 0;
    memset(&string[1], 0, 0x3FC);  // 1020 bytes
    WORD v5 = 0;
    char v6 = 0;

    va_list ap;
    va_start(ap, format);
    vsprintf(string, format, ap);
    va_end(ap);

    // Add to circular buffer with color
    LogTextAdd(color, string, (int)strlen(string));

    // Repaint parent window if available
    if (m_hParentWnd)
        LogTextPaintProc(m_hParentWnd);

    // Write to file if enabled
    if (m_bEnableFile && m_pLogFile)
        fprintf(m_pLogFile, "%s\n", string);
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

    // Format message
    char string[1021];
    string[0] = 0;
    memset(&string[1], 0, 0x3FC);  // 1020 bytes
    WORD v10 = 0;
    char v11 = 0;

    va_list ap;
    va_start(ap, format);

    // Get current time
    time_t timeptr;
    time(&timeptr);
    tm* v2 = localtime(&timeptr);

    // Determine AM/PM
    char noonStr[3] = "AM";
    if (v2->tm_hour >= 12)
    {
        strcpy(noonStr, "PM");
        v2->tm_hour -= 12;
    }
    if (v2->tm_hour == 0)
        v2->tm_hour = 12;

    // Format time string: "HH:MM:SS AM/PM "
    char* ascStr = asctime(v2);
    char v8[512];
    wsprintfA(v8, "%.8s%s ", ascStr + 11, noonStr);

    // Format user message
    vsprintf(string, format, ap);
    va_end(ap);

    // Combine
    strcat(v8, string);

    // Add to buffer
    LogTextAdd(LOG_COLOR_NORMAL, v8, (int)strlen(v8));

    // Write to file if enabled
    if (m_bEnableFile && m_pLogFile)
        fprintf(m_pLogFile, "%s\n", v8);
}

// =========================================================================
// LogTextPaintProc - Paint Buffered Text to Window
// =========================================================================
void CLogProc::LogTextPaintProc(HWND hWnd)
{
    if (!m_bEnableUI)
        return;

    // Clear the background (no parameter - uses m_hWnd internally)
    ClearParentWindow();

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
    if (m_hBufferBitmap)
    {
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

