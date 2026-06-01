#pragma once

#ifndef __LOGTOFILE_H__
#define __LOGTOFILE_H__

// Path/filename buffer sizes (MAX_PATH = 260 on Windows)
#define LOG_PATH_BUFFER_SIZE  260
#define LOG_PREFIX_BUFFER_SIZE 260
#define LOG_MESSAGE_BUFFER_SIZE 1024

class CLogToFile
{
public:
    // Constructor / Destructor
    CLogToFile(const char* szLogPrefix, const char* szLogPath, int bWithTime = 1);
    ~CLogToFile();

    // Logging output (printf-style, thread-safe)
    void Output(const char* format, ...);

    // Configuration
    void SetLogPrefix(const char* szPrefix);
    void SetLogPath(const char* szPath);
    void SetWithTime(int bWithTime);

    // Accessors
    const char* GetLogPrefix() const { return m_szLogPrefix; }
    const char* GetLogPath() const { return m_szLogPath; }
    BOOL IsWithTime() const { return m_bWithTime; }
    CRITICAL_SECTION* GetCriticalSection() { return &m_cs; }

protected:
    // Member variables (functional equivalent of original layout)

    // [0x000] Temporary FILE* handle (used only during Output)
    FILE* m_pFile;

    // [0x004] Log filename prefix buffer (e.g., "ErrorLog")
    char m_szLogPrefix[LOG_PREFIX_BUFFER_SIZE];

    // [0x108] Log directory path buffer (e.g., "log")
    char m_szLogPath[LOG_PATH_BUFFER_SIZE];

    // [0x20C] Flag: prepend time to each log line
    int m_bWithTime;

    // [0x210] Buffer for formatted message (vsprintf output)
    char m_szMessageBuffer[LOG_MESSAGE_BUFFER_SIZE];

    // [0x614] Thread synchronization
    CRITICAL_SECTION m_cs;

private:
    // Helper: Build date-based log filename
    void BuildLogFilename(char* szBuffer, size_t bufferSize, const SYSTEMTIME* st);

    // Helper: Write message to file with optional timestamp
    void WriteToFile(FILE* pFile, const char* szMessage, const SYSTEMTIME* st);
};
extern CLogToFile g_LogFileError;

#endif // __LOGTOFILE_H__