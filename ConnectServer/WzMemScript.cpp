#include "stdafx.h"
#include "WzMemScript.h"

// =========================================================================
// Constructor / Destructor
// =========================================================================
CWzMemScript::CWzMemScript()
    : m_pBuffer(NULL)
    , m_iBufferSize(0)
    , m_iBufferCount(0)
    , m_TokenNumber(0.0f)
    , m_CurrentToken(END)
{
    m_TokenString[0] = '\0';
}

CWzMemScript::~CWzMemScript()
{
    // Buffer is owned by caller - we don't free it
    m_pBuffer = NULL;
    m_iBufferSize = 0;
    m_iBufferCount = 0;
}

// =========================================================================
// SetBuffer - Initialize Parser with Memory Buffer
// Returns: 1 on success
// =========================================================================
int CWzMemScript::SetBuffer(char* buffer, int bufferSize)
{
    if (!buffer || bufferSize <= 0)
        return 0;

    SetScriptParsingBuffer(buffer, bufferSize);
    return 1;
}

// =========================================================================
// SetScriptParsingBuffer - Internal Buffer Setup
// =========================================================================
void CWzMemScript::SetScriptParsingBuffer(char* buffer, int size)
{
    m_pBuffer = buffer;
    m_iBufferSize = size;
    m_iBufferCount = 0;
    m_TokenNumber = 0.0f;
    m_TokenString[0] = '\0';
    m_CurrentToken = END;
}

// =========================================================================
// GetC - Read Next Character from Buffer
// Returns: Character code or EOF
// =========================================================================
int CWzMemScript::GetC()
{
    if (m_iBufferCount >= m_iBufferSize)
        return EOF;

    return static_cast<unsigned char>(m_pBuffer[m_iBufferCount++]);
}

// =========================================================================
// UnGetC - Put Character Back into Buffer
// =========================================================================
void CWzMemScript::UnGetC(int ch)
{
    if (m_iBufferCount <= 0)
        return;

    --m_iBufferCount;
    // Only restore if it matches (safety check)
    if (static_cast<unsigned char>(m_pBuffer[m_iBufferCount]) == static_cast<unsigned char>(ch))
    {
        // Character already in buffer, nothing to do
    }
    else
    {
        // Restore character (shouldn't normally happen)
        m_pBuffer[m_iBufferCount] = static_cast<char>(ch);
    }
}

// =========================================================================
// GetToken - Parse Next Token from Buffer
// Returns: SMDToken enum value
// Side Effects: Sets m_TokenNumber, m_TokenString, m_CurrentToken
// =========================================================================
SMDToken CWzMemScript::GetToken()
{
    char ch;
    m_TokenString[0] = '\0';

    // Skip whitespace and comments
    do
    {
        ch = static_cast<char>(GetC());
        if (ch == EOF)
            return m_CurrentToken = END;

        // Handle // comments
        if (ch == '/')
        {
            int next = GetC();
            if (next == '/')
            {
                // Skip to end of line or EOF
                while (true)
                {
                    ch = static_cast<char>(GetC());
                    if (ch == '\n' || ch == EOF)
                        break;
                }

                if (ch == EOF)
                    return m_CurrentToken = END;

                continue;  // Continue skipping whitespace after comment
            }
            else
            {
                // Not a comment - put back and process '/'
                UnGetC(next);
                ch = '/';
                // Fall through to process '/' as regular character
            }
        }
    } while (isspace(static_cast<unsigned char>(ch)));

    // Parse token based on first character
    char* p;
    char TempString[100];

    switch (ch)
    {
    case '#':
        return m_CurrentToken = COMMAND;

    case ';':
        return m_CurrentToken = SEMICOLON;

    case ',':
        return m_CurrentToken = COMMA;

    case '{':
        return m_CurrentToken = LP;

    case '}':
        return m_CurrentToken = RP;

        // Numeric token: digits, decimal point, minus sign
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
    case '.': case '-':
    {
        UnGetC(ch);
        p = TempString;

        int c;
        while ((c = GetC()) != EOF)
        {
            if (c != '.' && !isdigit(c) && c != '-')
                break;
            *p++ = static_cast<char>(c);
        }
        *p = '\0';

        m_TokenNumber = static_cast<float>(atof(TempString));

        // Put back non-numeric character
        if (c != EOF)
            UnGetC(c);

        return m_CurrentToken = NUMBER;
    }

    // Quoted string
    case '"':
    {
        p = m_TokenString;

        int c;
        while ((c = GetC()) != EOF && c != '"')
        {
            *p++ = static_cast<char>(c);
        }
        *p = '\0';

        // Handle unterminated string
        if (c != '"')
        {
            UnGetC(c);
        }

        return m_CurrentToken = NAME;
    }

    // Identifier: starts with letter
    default:
        if (isalpha(static_cast<unsigned char>(ch)))
        {
            p = m_TokenString;
            *p++ = ch;

            int c;
            while ((c = GetC()) != EOF)
            {
                if (c != '.' && c != '_' && !isalnum(static_cast<unsigned char>(c)))
                    break;
                *p++ = static_cast<char>(c);
            }
            *p = '\0';

            // Put back non-identifier character
            if (c != EOF)
                UnGetC(c);

            return m_CurrentToken = NAME;
        }

        // Unrecognized character
        return m_CurrentToken = SMD_ERROR;
    }
}