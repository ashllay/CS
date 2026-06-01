#include "stdafx.h"
#include "LogToFile.h"

static const char* s_szDefaultLogPath = "log";
static const char* s_szDefaultLogPrefix = "LogData";
static const char* s_szAppendMode = "a";

// =========================================================================
// Constructor
// =========================================================================
CLogToFile::CLogToFile(const char* szLogPrefix, const char* szLogPath, int bWithTime)
    : m_pFile(NULL)
    , m_bWithTime(bWithTime)
{
    // Initialize log prefix (use default if empty)
    if (!szLogPrefix || !szLogPrefix[0] || strcmp(szLogPrefix, "") == 0)
    {
        strncpy_s(m_szLogPrefix, sizeof(m_szLogPrefix), s_szDefaultLogPrefix, _TRUNCATE);
    }
    else
    {
        strncpy_s(m_szLogPrefix, sizeof(m_szLogPrefix), szLogPrefix, _TRUNCATE);
    }

    // Initialize log path (use default if empty)
    if (!szLogPath || !szLogPath[0] || strcmp(szLogPath, "") == 0)
    {
        strncpy_s(m_szLogPath, sizeof(m_szLogPath), s_szDefaultLogPath, _TRUNCATE);
    }
    else
    {
        strncpy_s(m_szLogPath, sizeof(m_szLogPath), szLogPath, _TRUNCATE);
    }

    // Create log directory if it doesn't exist
    CreateDirectoryA(m_szLogPath, NULL);

    // Initialize critical section for thread-safe logging
    InitializeCriticalSection(&m_cs);

    // Initialize message buffer
    ZeroMemory(m_szMessageBuffer, sizeof(m_szMessageBuffer));
}

// =========================================================================
// Destructor
// =========================================================================
CLogToFile::~CLogToFile()
{
    // Clean up synchronization object
    DeleteCriticalSection(&m_cs);
}

// =========================================================================
// SetLogPrefix - Update Log Filename Prefix
// =========================================================================
void CLogToFile::SetLogPrefix(const char* szPrefix)
{
    if (szPrefix && szPrefix[0])
    {
        strncpy_s(m_szLogPrefix, sizeof(m_szLogPrefix), szPrefix, _TRUNCATE);
    }
}

// =========================================================================
// SetLogPath - Update Log Directory Path
// =========================================================================
void CLogToFile::SetLogPath(const char* szPath)
{
    if (szPath && szPath[0])
    {
        strncpy_s(m_szLogPath, sizeof(m_szLogPath), szPath, _TRUNCATE);
        // Create directory if needed
        CreateDirectoryA(m_szLogPath, NULL);
    }
}

// =========================================================================
// SetWithTime - Enable/Disable Timestamp Prefix
// =========================================================================
void CLogToFile::SetWithTime(int bWithTime)
{
    m_bWithTime = (bWithTime != 0);
}

// =========================================================================
// BuildLogFilename - Helper: Create Date-Based Log Filename
// Format: path\prefix YYYY-MM-DD.txt
// =========================================================================
void CLogToFile::BuildLogFilename(char* szBuffer, size_t bufferSize, const SYSTEMTIME* st)
{
    if (!szBuffer || !st)
        return;

    // Format: "log\ErrorLog 2026-05-18.txt"
    _snprintf_s(szBuffer, bufferSize, _TRUNCATE,
        "%s\\%s %04d-%02d-%02d.txt",
        m_szLogPath,
        m_szLogPrefix,
        st->wYear,
        st->wMonth,
        st->wDay);
}

// =========================================================================
// WriteToFile - Helper: Write Message with Optional Timestamp
// =========================================================================
void CLogToFile::WriteToFile(FILE* pFile, const char* szMessage, const SYSTEMTIME* st)
{
    if (!pFile || !szMessage || !st)
        return;

    if (m_bWithTime)
    {
        // Format: "HH:MM:SS  message\n"
        fprintf(pFile, "%02d:%02d:%02d  %s\n",
            st->wHour, st->wMinute, st->wSecond,
            szMessage);
    }
    else
    {
        // Format: "message\n"
        fprintf(pFile, "%s\n", szMessage);
    }
}

// =========================================================================
// Output - Thread-Safe Logging to File
// =========================================================================
void CLogToFile::Output(const char* format, ...)
{
    if (!format)
        return;

    // Format message using variable arguments
    va_list args;
    va_start(args, format);
    _vsnprintf_s(m_szMessageBuffer, sizeof(m_szMessageBuffer), _TRUNCATE, format, args);
    va_end(args);

    // Thread-safe section
    EnterCriticalSection(&m_cs);

    // Get current local time for filename and optional timestamp
    SYSTEMTIME st;
    GetLocalTime(&st);

    // Build log filename: path\prefix YYYY-MM-DD.txt
    char szLogFilename[MAX_PATH];
    BuildLogFilename(szLogFilename, sizeof(szLogFilename), &st);

    // Open file in append mode (original: fopen with "a")
    FILE* pFile = fopen(szLogFilename, "a+");
    if (pFile)
    {
        // Write message with optional timestamp
        WriteToFile(pFile, m_szMessageBuffer, &st);

        // Close file immediately (original behavior - not keeping handle open)
        fclose(pFile);
    }

    LeaveCriticalSection(&m_cs);
}