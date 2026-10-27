#pragma once

#ifndef __LOGTOLISTBOX_H__
#define __LOGTOLISTBOX_H__
#include "StdAfx.h"

// Buffer sizes (original used 64KB embedded buffers - we use safe sizes)
#define LOG_FORMAT_BUFFER_SIZE  1024    // For vsprintf formatting
#define LOG_OUTPUT_BUFFER_SIZE  1024    // For timestamped output
#define MAX_LISTBOX_ITEMS       10000   // Auto-scroll limit

class CLogToListBox
{
public:
    // Constructor / Destructor
    CLogToListBox(int bWithDate = 1, HWND hListBox = NULL);
    ~CLogToListBox();

    // Configuration
    void SetListBox(HWND hListBox);
    void SetWithDate(int bWithDate);

    // Logging output (printf-style)
    void Output(const char* format, ...);

    // Utility
    void ResetContents();

    // Accessors
    HWND GetListBox() const { return m_hListBox; }
    BOOL IsWithDate() const { return m_bWithDate; }
    CRITICAL_SECTION* GetCriticalSection() { return &m_LogToListBoxcs; }

protected:
    // Member variables (functional equivalent of original layout)

    // Format buffer for vsprintf (original: offset 0, 64KB)
    char m_szFormatBuffer[LOG_FORMAT_BUFFER_SIZE];

    // Output buffer for timestamped text (original: offset 0x10000, 64KB)
    char m_szOutputBuffer[LOG_OUTPUT_BUFFER_SIZE];

    // Target ListBox control handle (original: offset 0x20000)
    HWND m_hListBox;

    // Flag: prepend timestamp to output (original: offset 0x20004)
    int m_bWithDate;

    // Thread synchronization (original: offset 0x20008)
    CRITICAL_SECTION m_LogToListBoxcs;

private:
    // Helper: Format message with optional timestamp
    void FormatOutput(const char* szMessage);

    // Helper: Auto-scroll listbox to bottom
    void AutoScroll();
};
extern CLogToListBox g_LogListBoxError;
#endif // __LOGTOLISTBOX_H__