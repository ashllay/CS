#include "StdAfx.h"
#include "WzTcpSock.h"
#include "LogProc.h"

// Static member definitions
CRITICAL_SECTION CWzTcpSock::m_critSOCKET_LPOBJ = { NULL, 0, 0, NULL, NULL, 0 };
WNDPROC CWzTcpSock::m_lpOldWndProc = NULL;
std::map<SOCKET, CWzTcpSock*> CWzTcpSock::m_mapSOCKET_LPOBJ;

// Static format strings
static const char* s_szSendBufferFull = "Send buffer is full";

// =========================================================================
// InitializeWzTcpSock
// =========================================================================
int CWzTcpSock::InitializeWzTcpSock()
{
    WSADATA WSAData;
    int v0 = WSAStartup(MAKEWORD(2, 2), &WSAData);

    if (v0)
    {
        switch (v0)
        {
        case WSAEFAULT:          g_LogProc.LogAddFuncColor(2, "WSAEFAULT"); break;
        case WSAEINPROGRESS:     g_LogProc.LogAddFuncColor(2, "WSAEINPROGRESS"); break;
        case WSAEPROCLIM:        g_LogProc.LogAddFuncColor(2, "WSAEPROCLIM"); break;
        case WSASYSNOTREADY:     g_LogProc.LogAddFuncColor(2, "WSASYSNOTREADY"); break;
        case WSAVERNOTSUPPORTED: g_LogProc.LogAddFuncColor(2, "WSAVERNOTSUPPORTED"); break;
        default: break;
        }
        WSACleanup();
        return 0;
    }

    InitializeCriticalSection(&m_critSOCKET_LPOBJ);
    return 1;
}

// =========================================================================
// EndWzTcpSock
// =========================================================================
int CWzTcpSock::EndWzTcpSock()
{
    int result = WSACleanup();

    if (result != 0)
    {
        int err = WSAGetLastError();
        if (err == WSANOTINITIALISED)  // 10093
        {
            g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "WSAStartup not initialized");
        }
        else if (err == WSAEINPROGRESS)  // 10036
        {
            g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "WSAStartup already called");
        }
        else
        {
            g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "Unknown WSA error");
        }
        return 0;
    }

    DeleteCriticalSection(&m_critSOCKET_LPOBJ);
    return 1;
}

// =========================================================================
// Constructor
// =========================================================================
CWzTcpSock::CWzTcpSock()
{
    m_socket = INVALID_SOCKET;
    m_lpRecvProc = NULL;
    m_lpEventProc = NULL;

    memset(m_szIpAddress, 0, sizeof(m_szIpAddress));
    m_Port = 0;
    m_hWnd = NULL;

    m_SendSize = 0;
    m_RecvSize = 0;
    m_pUserData = NULL;
    m_LastError = 0;
    m_bShowError = 0;
    m_SocketType = SOCKET_MODE_NONE;

    memset(m_szError, 0, sizeof(m_szError));

    InitializeCriticalSection(&m_csRecv);
    InitializeCriticalSection(&m_csSend);
    InitializeCriticalSection(&m_csEtc);
}

// =========================================================================
// Destructor
// =========================================================================
CWzTcpSock::~CWzTcpSock()
{
    if (m_socket != INVALID_SOCKET)
    {
        Disconnect();
        m_socket = INVALID_SOCKET;
    }

    DeleteCriticalSection(&m_csEtc);
    DeleteCriticalSection(&m_csSend);
    DeleteCriticalSection(&m_csRecv);
}

// =========================================================================
// Initialize
// =========================================================================
void CWzTcpSock::Initialize(HWND hWnd, void* userData,
    RECVPROC recvProc, EVENTPROC eventProc)
{
    m_pUserData = userData;
    m_hWnd = hWnd;
    m_lpRecvProc = recvProc;
    m_lpEventProc = eventProc;

    if (IsWindow(hWnd))
    {
        if (!m_lpOldWndProc)
        {
            m_lpOldWndProc = (WNDPROC)SetWindowLongA(
                m_hWnd, GWLP_WNDPROC, (LONG)CWzTcpSock::WZ_SM_WndProc);
        }
    }
    else
    {
        g_LogProc.LogAddFuncColor(2, "Invalid notify window");
        m_hWnd = NULL;
    }
}

// =========================================================================
// Connect
// =========================================================================
int CWzTcpSock::Connect(char* ip, WORD port)
{
    if (m_socket != INVALID_SOCKET)
        return 0;

    int hasAddr = strcmp(ip, "") != 0;
    if (hasAddr)
        strcpy(m_szIpAddress, ip);

    if (port)
        m_Port = port;

    // Validate parameters
    if (!hasAddr || !m_Port || !m_hWnd)
    {
        if (!m_lpEventProc)
            goto LABEL_MISSING;
        if (!m_lpRecvProc)
        {
        LABEL_MISSING:
            const char* addrOk = hasAddr ? "OK" : "MISSING";
            const char* portOk = m_Port ? "OK" : "MISSING";
            const char* wndOk = m_hWnd ? "OK" : "MISSING";
            const char* recvOk = m_lpRecvProc ? "OK" : "MISSING";
            const char* otherOk = m_lpEventProc ? "OK" : "MISSING";

            char msg[128];
            wsprintfA(msg, "Addr:%s Port:%s Wnd:%s Recv:%s Other:%s",
                addrOk, portOk, wndOk, recvOk, otherOk);
            SavePrivateErrorMessage(msg);
            goto LABEL_ERROR;
        }
    }

    // Create socket
    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET)
    {
        SaveLastError(0);
        Disconnect();
        goto LABEL_ERROR;
    }

    // Setup address
    sockaddr_in name = { 0 };
    name.sin_family = AF_INET;
    name.sin_port = htons(m_Port);
    name.sin_addr.s_addr = inet_addr(m_szIpAddress);

    // Async select for FD_CONNECT only
    if (WSAAsyncSelect(m_socket, m_hWnd, WM_SOCKET_EVENT, FD_CONNECT) == SOCKET_ERROR)
    {
        SaveLastError(0);
        goto LABEL_DISCONNECT;
    }

    // Connect
    if (connect(m_socket, (sockaddr*)&name, sizeof(name)) == SOCKET_ERROR)
    {
        int Error = WSAGetLastError();
        if (Error != WSAEWOULDBLOCK)
        {
            SaveLastError(Error);
            Disconnect();
            goto LABEL_ERROR;
        }
    }

    m_SocketType = SOCKET_MODE_CONNECT;

    EnterCriticalSection(&m_critSOCKET_LPOBJ);
    m_mapSOCKET_LPOBJ[m_socket] = this;
    LeaveCriticalSection(&m_critSOCKET_LPOBJ);

    return 1;

LABEL_DISCONNECT:
    Disconnect();
LABEL_ERROR:
    DiplayErrorMsgBox();
    return 0;
}

// =========================================================================
// Accept
// =========================================================================
int CWzTcpSock::Accept(WORD port)
{
    if (m_socket != INVALID_SOCKET)
        return 0;

    m_Port = port;

    if (!port || !m_hWnd)
        goto LABEL_MISSING;
    if (!m_lpEventProc)
        goto LABEL_MISSING;
    if (!m_lpRecvProc)
    {
    LABEL_MISSING:
        const char* portOk = port ? "OK" : "MISSING";
        const char* wndOk = m_hWnd ? "OK" : "MISSING";
        const char* recvOk = m_lpRecvProc ? "OK" : "MISSING";
        const char* otherOk = m_lpEventProc ? "OK" : "MISSING";

        char msg[128];
        wsprintfA(msg, "Port:%s Wnd:%s Recv:%s Other:%s",
            portOk, wndOk, recvOk, otherOk);
        SavePrivateErrorMessage(msg);
        DiplayErrorMsgBox();
        return 0;
    }

    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET)
    {
    LABEL_ERROR:
        SaveLastError(0);
        Disconnect();
        DiplayErrorMsgBox();
        return 0;
    }

    sockaddr_in name = { 0 };
    name.sin_family = AF_INET;
    name.sin_port = htons(m_Port);
    name.sin_addr.s_addr = INADDR_ANY;

    if (bind(m_socket, (sockaddr*)&name, sizeof(name)) == SOCKET_ERROR)
        goto LABEL_ERROR;

    if (listen(m_socket, SOMAXCONN) == SOCKET_ERROR)
        goto LABEL_ERROR;

    if (WSAAsyncSelect(m_socket, m_hWnd, WM_SOCKET_EVENT, FD_ACCEPT) == SOCKET_ERROR)
        goto LABEL_ERROR;

    m_SocketType = SOCKET_MODE_LISTEN;

    EnterCriticalSection(&m_critSOCKET_LPOBJ);
    m_mapSOCKET_LPOBJ[m_socket] = this;
    LeaveCriticalSection(&m_critSOCKET_LPOBJ);

    return 1;
}

// =========================================================================
// Disconnect
// =========================================================================
int CWzTcpSock::Disconnect()
{
    if (m_socket != INVALID_SOCKET)
    {
        EnterCriticalSection(&m_critSOCKET_LPOBJ);
        m_mapSOCKET_LPOBJ.erase(m_socket);
        LeaveCriticalSection(&m_critSOCKET_LPOBJ);

        WSAAsyncSelect(m_socket, m_hWnd, 0, 0);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    return 1;
}

// =========================================================================
// FillSocketInfo
// =========================================================================
void CWzTcpSock::FillSocketInfo(SOCKET s, int type, HWND hWnd, void* userData, RECVPROC recvProc, EVENTPROC eventProc)
{
    m_socket = s;
    m_lpRecvProc = recvProc;
    m_lpEventProc = eventProc;
    m_hWnd = hWnd;
    m_SocketType = type;
    m_pUserData = userData;
    m_SendSize = 0;
    m_RecvSize = 0;
    m_LastError = 0;
    m_bShowError = 0;
    memset(m_szError, 0, sizeof(m_szError));
}

// =========================================================================
// GetSocketEvent
// =========================================================================
int CWzTcpSock::GetSocketEvent(SOCKET s, LPARAM lParam)
{
    unsigned short nEvent = LOWORD(lParam);
    unsigned short nError = HIWORD(lParam);

    CWzTcpSock* pSock = NULL;
    EnterCriticalSection(&m_critSOCKET_LPOBJ);
    std::map<SOCKET, CWzTcpSock*>::iterator it = m_mapSOCKET_LPOBJ.find(s);
    if (it != m_mapSOCKET_LPOBJ.end())
        pSock = it->second;
    LeaveCriticalSection(&m_critSOCKET_LPOBJ);

    if (!pSock)
        return 0;

    switch (nEvent)
    {
    case FD_READ:
        if (nError)
        {
            pSock->SaveLastError(nError);
            pSock->Disconnect();
            pSock->DiplayErrorMsgBox();
            return 0;
        }
        return pSock->RecvPacket();

    case FD_WRITE:
        if (nError)
        {
            pSock->SaveLastError(nError);
            pSock->Disconnect();
            pSock->DiplayErrorMsgBox();
            return 0;
        }
        pSock->ResendOn_FD_WRITE();
        return 1;

    case FD_ACCEPT:
        if (nError)
        {
            pSock->SaveLastError(nError);
            pSock->Disconnect();
            pSock->Accept(pSock->m_Port);
            pSock->DiplayErrorMsgBox();
            if (pSock->m_lpEventProc)
                pSock->m_lpEventProc(pSock->m_socket, 3, pSock->m_pUserData, pSock);
            return 0;
        }
        else
        {
            sockaddr_in addr = { 0 };
            int addrlen = sizeof(addr);

            SOCKET accepted = accept(s, (sockaddr*)&addr, &addrlen);
            if (accepted == INVALID_SOCKET)
                return 0;

            if (WSAAsyncSelect(accepted, pSock->m_hWnd, WM_SOCKET_EVENT,
                FD_READ | FD_WRITE | FD_CLOSE) == SOCKET_ERROR)
            {
                pSock->SaveLastError(0);
                closesocket(accepted);
                pSock->DiplayErrorMsgBox();
                if (pSock->m_lpEventProc)
                    pSock->m_lpEventProc(pSock->m_socket, 3, pSock->m_pUserData, pSock);
                return 0;
            }

            CWzTcpSock* pNewSock = new CWzTcpSock();
            if (pNewSock)
            {
                pNewSock->FillSocketInfo(accepted, SOCKET_MODE_ACCEPTED,
                    pSock->m_hWnd, NULL, pSock->m_lpRecvProc, pSock->m_lpEventProc);

                EnterCriticalSection(&m_critSOCKET_LPOBJ);
                m_mapSOCKET_LPOBJ[accepted] = pNewSock;
                LeaveCriticalSection(&m_critSOCKET_LPOBJ);
            }
            return 1;
        }

    case FD_CONNECT:
        if (nError)
        {
            pSock->SaveLastError(nError);
            pSock->Disconnect();
            pSock->DiplayErrorMsgBox();
            if (pSock->m_lpEventProc)
                pSock->m_lpEventProc(pSock->m_socket, 3, pSock->m_pUserData, pSock);
            return 0;
        }
        else
        {
            if (WSAAsyncSelect(pSock->m_socket, pSock->m_hWnd, WM_SOCKET_EVENT,
                FD_READ | FD_WRITE | FD_CLOSE) != SOCKET_ERROR)
            {
                int nBufSize = 16392;
                if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char*)&nBufSize, sizeof(nBufSize)) != SOCKET_ERROR &&
                    setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char*)&nBufSize, sizeof(nBufSize)) != SOCKET_ERROR)
                {
                    if (pSock->m_lpEventProc)
                        pSock->m_lpEventProc(pSock->m_socket, 0, pSock->m_pUserData, pSock);
                    return 1;
                }
            }
            pSock->SaveLastError(0);
            pSock->Disconnect();
            pSock->DiplayErrorMsgBox();
            return 0;
        }

    case FD_CLOSE:
        if (pSock->m_lpEventProc)
            pSock->m_lpEventProc(pSock->m_socket, 1, pSock->m_pUserData, pSock);

        if (pSock->m_SocketType == SOCKET_MODE_ACCEPTED)
        {
            pSock->Disconnect();
            delete pSock;
        }
        return 1;

    default:
        if (nError)
        {
            pSock->SaveLastError(nError);
            pSock->Disconnect();
            pSock->DiplayErrorMsgBox();
            return 0;
        }
        return 1;
    }
}

// =========================================================================
// RecvPacket
// =========================================================================
int CWzTcpSock::RecvPacket()
{
    EnterCriticalSection(&m_csRecv);

    int ret = recv(m_socket,
        (char*)&m_RecvBuffer[m_RecvSize],
        SOCKET_BUFFER_SIZE - m_RecvSize, 0);

    if (ret == SOCKET_ERROR)
    {
        int Error = WSAGetLastError();
        if (Error != WSAEWOULDBLOCK)
        {
            SaveLastError(Error);
            Disconnect();
            DiplayErrorMsgBox();
            LeaveCriticalSection(&m_csRecv);
            return 0;
        }
        LeaveCriticalSection(&m_csRecv);
        return 1;
    }

    int totalBytes = m_RecvSize + ret;
    int offset = 0;

    while (true)
    {
        BYTE* lpMsg = (BYTE*)&m_RecvBuffer[offset];

        int size = 0;
        BYTE head = 0;
        BYTE sub = 0;

        if (lpMsg[0] == 0xC1)
        {
            size = lpMsg[1];
            head = lpMsg[2];
            sub = lpMsg[3];
        }
        else if (lpMsg[0] == 0xC2)
        {
            size = lpMsg[2] + (lpMsg[1] << 8);
            head = lpMsg[3];
            sub = lpMsg[4];  // Or wherever the sub-byte is
        }

        if (size <= 0)
        {
            m_RecvSize = 0;
            LeaveCriticalSection(&m_csRecv);
            return 1;
        }

        if (size <= totalBytes)
        {
            if (m_lpRecvProc)
            {
                m_lpRecvProc(m_socket, m_pUserData, head, sub, (char*)lpMsg, size);
            }

            offset += size;
            totalBytes -= size;
        }
        else
        {
            break;
        }
    }

    if (offset > 0 && totalBytes > 0)
    {
        memmove(m_RecvBuffer, m_RecvBuffer + offset, totalBytes);
    }

    m_RecvSize = totalBytes;
    LeaveCriticalSection(&m_csRecv);
    return 1;
}

// =========================================================================
// SendPacket
// =========================================================================
int CWzTcpSock::SendPacket(char* buf, int len)
{
    int nRemaining = len;
    int nResult = ResendOn_FD_WRITE();
    int nSent = 0;  // DECLARE HERE

    if (nResult == 2)
    {
        if (SOCKET_BUFFER_SIZE - m_SendSize <= len)
        {
            memcpy(m_SendBuffer + m_SendSize, buf, len);
            m_SendSize += len;
            return 1;
        }
        goto LABEL_BUFFER_FULL;
    }

    if (!nResult)
        return 0;

    while (nRemaining > 0)
    {
        nSent = send(m_socket, buf, nRemaining, 0);
        if (nSent <= 0)
            break;

        nRemaining -= nSent;
        if (!nRemaining)
            return 1;
        buf += nSent;
    }

    if (nSent != SOCKET_ERROR)
    {
    LABEL_DISCONNECT:
        Disconnect();
        return 0;
    }

    int Error = WSAGetLastError();
    if (Error != WSAEWOULDBLOCK)
    {
        SaveLastError(Error);
        Disconnect();
        DiplayErrorMsgBox();
        return 0;
    }

    if (SOCKET_BUFFER_SIZE - m_SendSize > nRemaining)
    {
    LABEL_BUFFER_FULL:
        SavePrivateErrorMessage((char*)s_szSendBufferFull);
        goto LABEL_DISCONNECT;
    }

    memcpy(m_SendBuffer + m_SendSize, buf, nRemaining);
    m_SendSize += nRemaining;
    return 1;
}

// =========================================================================
// ResendOn_FD_WRITE
// =========================================================================
int CWzTcpSock::ResendOn_FD_WRITE()
{
    int nSize = m_SendSize;
    const char* pBuf = m_SendBuffer;
    int nSent = 0;  // DECLARE HERE

    if (nSize <= 0)
    {
        if (nSize < 0)
            m_SendSize = 0;
        return 1;
    }

    while (nSize > 0)
    {
        nSent = send(m_socket, pBuf, nSize, 0);
        if (nSent <= 0)
            break;

        nSize -= nSent;
        if (!nSize)
        {
            m_SendSize = 0;
            return 1;
        }
        pBuf += nSent;
    }

    if (nSent == SOCKET_ERROR)
    {
        int Error = WSAGetLastError();
        if (Error == WSAEWOULDBLOCK)
        {
            memmove(m_SendBuffer, pBuf, nSize);
            m_SendSize = nSize;
            return 2;
        }
        else
        {
            SaveLastError(Error);
            Disconnect();
            DiplayErrorMsgBox();
            return 0;
        }
    }
    else
    {
        Disconnect();
        return 0;
    }
}

// =========================================================================
// SaveLastError
// =========================================================================
void CWzTcpSock::SaveLastError(int err)
{
    if (!err)
        err = WSAGetLastError();

    m_LastError = err;

    FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, m_LastError, 0, m_szError, sizeof(m_szError), NULL);

    if (m_lpEventProc)
        m_lpEventProc(m_socket, 2, m_pUserData, this);
}

// =========================================================================
// SavePrivateErrorMessage
// =========================================================================
void CWzTcpSock::SavePrivateErrorMessage(char* msg)
{
    m_LastError = 0;
    strcpy(m_szError, msg);

    if (m_lpEventProc)
        m_lpEventProc(m_socket, 2, m_pUserData, this);
}

// =========================================================================
// DiplayErrorMsgBox
// =========================================================================
void CWzTcpSock::DiplayErrorMsgBox()
{
    OutputDebugStringA(m_szError);

    if (m_bShowError)
        g_LogProc.LogAddFuncColor(2, m_szError);
}

// =========================================================================
// SetErrorMsgFlag
// =========================================================================
void CWzTcpSock::SetErrorMsgFlag(int flag)
{
    m_bShowError = flag;
}

// =========================================================================
// WZ_SM_WndProc
// =========================================================================
LRESULT CALLBACK CWzTcpSock::WZ_SM_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    if (Msg == WM_SOCKET_EVENT)
        GetSocketEvent((SOCKET)wParam, lParam);

    return CallWindowProcA(m_lpOldWndProc, hWnd, Msg, wParam, lParam);
}