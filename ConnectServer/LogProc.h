#pragma once

#ifndef __LOGPROC_H__
#define __LOGPROC_H__

#include <windows.h>
#include <cstdio>

// Circular buffer constants (matching original IDA)
#define LOG_BUFFER_COUNT    30      // Number of log lines kept in memory (original: 25)
#define LOG_BUFFER_SIZE     80      // Max characters per line (original: 0x50 = 80)

// Logging color/type codes (original enum values)
#define LOG_COLOR_NORMAL    0       // Black text
#define LOG_COLOR_RED       2       // Red (0x0000FF)
#define LOG_COLOR_GREEN     3       // Green (0x006400)
#define LOG_COLOR_BLUE      4       // Blue (0xFF0000)
#define LOG_COLOR_GRAY      5       // Gray (0x00009B)
#define LOG_COLOR_DARK_RED  6       // Dark Red (0x640000)

class CLogProc
{
public:
    // Constructor / Destructor
    CLogProc(int nTextPosX, int nTextPosY, HWND hWndParent);
    ~CLogProc();

    // Initialization
    void LogInit(int bEnable);
    void LogClose();

    // File management
    const char* LogGetFileName();
    void LogDataSet();          // Open new log file with current date
    int  LogDateChange();       // Check date, rotate file if needed

    // Adding log entries
    void LogAddFunc(const char* format, ...);
    void LogAddFuncColor(BYTE color, const char* format, ...);
    void LogAddHeadHexFunc(int type, char* data, int len);
    void LogAddTimeDateFunc(const char* format, ...);

    // Text buffer management
    void LogTextAdd(BYTE color, const char* text, int len);
    void LogTextClear();

    // UI rendering
    void LogTextPaintProc(HWND hWnd);
    void ClearParentWindow();
    void SetParentWindowBitmap();

    // Error handling
    static void MsgBox(const char* format, ...);
    void SetLogWindow(HWND hWnd)
    {
        m_hParentWnd = hWnd;
    }
//private:
    // Member variables - layout MUST match original binary offsets

    // [0x000] Virtual function table pointer (compiler-managed)

    // [0x004] Log filename buffer (128 bytes)
    char m_szLogFileName[128];

    // [0x084] Padding (32 bytes to align to next boundary)

    // [0x104] File handle for log output (original offset: this[65])
    FILE* m_pLogFile;

    // [0x108] Reserved/padding (original doesn't use this slot)

    // [0x10C] Current buffer index - circular buffer write position (original: this[66])
    int m_nBufferIndex;

    // [0x110] Circular buffer: 25 entries x 80 bytes each = 2000 bytes
    char m_szLogBuffer[LOG_BUFFER_COUNT][LOG_BUFFER_SIZE];

    // [0x8DC] Text length array: 25 entries x 2 bytes (WORD) each = 50 bytes (original offset: +1334 words)
    WORD m_wTextLength[LOG_BUFFER_COUNT];

    // [0x90E] Color/type array: 25 entries x 1 byte each = 25 bytes (original offset: +2728 bytes)
    BYTE m_bTextColor[LOG_BUFFER_COUNT];

    // [0x927] Padding byte

    // [0x928] Enable UI logging flag (original: this[690])
    int m_bEnableUI;

    // [0x92C] Enable file logging flag (original: this[691])
    int m_bEnableFile;

    // [0x930] Log file date tracking - Year (original: this[693])
    int m_nLogYear;

    // [0x934] Log file date tracking - Month (original: this[694])
    int m_nLogMonth;

    // [0x938] Log file date tracking - Day (original: this[695])
    int m_nLogDay;

    // [0x93C] Text output X position for painting (original: this[696])
    int m_nTextPosX;

    // [0x940] Text output Y position for painting (original: this[697])
    int m_nTextPosY;

    // [0x944] Parent window handle for UI output (original: this[698])
    HWND m_hParentWnd;

    // [0x948] Offscreen bitmap handle for double-buffering (original: this[699])
    HBITMAP m_hBufferBitmap;
};

#endif // __LOGPROC_H__