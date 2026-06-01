#pragma once

#ifndef __WZMEMSCRIPT_H__
#define __WZMEMSCRIPT_H__

#include <windows.h>
#include <ctype.h>

// Token types (matches original SMDToken enum)
enum SMDToken
{
    NAME = 0,           // Identifier or string token
    NUMBER = 1,         // Numeric token
    END = 2,            // End of buffer/file
    COMMAND = '#',      // '#' command marker
    LP = '{',           // Left brace
    RP = '}',           // Right brace
    COMMA = ',',        // Comma separator
    SEMICOLON = ';',    // Semicolon separator
    SMD_ERROR = 60      // Unrecognized token
};

class CWzMemScript
{
public:
    // Constructor / Destructor
    CWzMemScript();
    virtual ~CWzMemScript();

    // Buffer setup
    int SetBuffer(char* buffer, int bufferSize);

    // Token retrieval
    SMDToken GetToken();
    SMDToken TokenType() const { return m_CurrentToken; }
    int GetNumber() const { return static_cast<int>(m_TokenNumber); }
    const char* GetString() const { return m_TokenString; }

    // Direct access (for advanced usage)
    float GetTokenNumber() const { return m_TokenNumber; }
    const char* GetTokenString() const { return m_TokenString; }

protected:
    // Character I/O abstraction
    int GetC();
    void UnGetC(int ch);

    // Internal buffer setup (called by SetBuffer)
    void SetScriptParsingBuffer(char* buffer, int size);

private:
    // Member variables
    char* m_pBuffer;            // Pointer to parsing buffer
    int m_iBufferSize;          // Total buffer size
    int m_iBufferCount;         // Current read position

    // Token state
    float m_TokenNumber;        // Parsed numeric value
    char m_TokenString[100];    // Parsed string/identifier
    SMDToken m_CurrentToken;    // Last token type returned
};

#endif // __WZMEMSCRIPT_H__