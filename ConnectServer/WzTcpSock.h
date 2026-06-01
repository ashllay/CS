#pragma once

#ifndef __WZTCPSOCK_H__
#define __WZTCPSOCK_H__

#include <windows.h>
#include <winsock2.h>
#include <map>

#pragma comment(lib, "ws2_32.lib")

// Socket mode flags
#define SOCKET_MODE_NONE     0
#define SOCKET_MODE_CONNECT  1
#define SOCKET_MODE_LISTEN   2
#define SOCKET_MODE_ACCEPTED 3

// Buffer sizes (matching original)
#define SOCKET_BUFFER_SIZE   20480       // 20KB (0x5000)
#define SOCKET_ADDRESS_SIZE  128
#define SOCKET_ERROR_SIZE    128

// Async socket message
#define WM_SOCKET_EVENT      0x432      // 1074

class CWzTcpSock
{
public:
    // Callback types
    typedef void(__cdecl* RECVPROC)(
        unsigned int socket,
        void* userData,
        BYTE head,
        BYTE subHead,
        char* lpMsg,
        int size);

    typedef void(__cdecl* EVENTPROC)(
        unsigned int socket,
        int eventType,
        void* userData,
        CWzTcpSock* lpSock);

public:
    CWzTcpSock();
    ~CWzTcpSock();

    // Static initialization
    static int  InitializeWzTcpSock();
    static int  EndWzTcpSock();

    // Instance initialization
    void Initialize(HWND hWnd, void* userData, RECVPROC recvProc, EVENTPROC eventProc);

    // Connection management
    int  Connect(char* ip, WORD port);
    int  Accept(WORD port);
    int  Disconnect();

    // Socket info setup (for accepted connections)
    void FillSocketInfo(SOCKET s, int type, HWND hWnd, void* userData,
        RECVPROC recvProc, EVENTPROC eventProc);

    // Static event handler
    static int GetSocketEvent(SOCKET s, LPARAM lParam);

    // Static window procedure
    static LRESULT CALLBACK WZ_SM_WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

    // Data transfer
    int  SendPacket(char* buf, int len);

    // Error handling
    void SetErrorMsgFlag(int flag);

protected:
    int  RecvPacket();
    int  ResendOn_FD_WRITE();

    void SaveLastError(int err = 0);
    void SavePrivateErrorMessage(char* msg);
    void DiplayErrorMsgBox();

private:
    // [0x0000] Socket handle
    SOCKET      m_socket;

    // [0x0004] Remote IP address string (128 bytes)
    char        m_szIpAddress[128];

    // [0x0084] Remote port
    WORD        m_Port;

    // [0x0088] Notify window handle (offset 34*4 = 136)
    HWND        m_hWnd;

    // [0x008C] Send buffer (20KB) - offset 140 bytes from start
    char        m_SendBuffer[SOCKET_BUFFER_SIZE];
    int         m_SendSize;         // Send buffer offset (10275)

    // [0x5090] Receive buffer used bytes (10276)
    int         m_RecvSize;

    // [0x5094] Receive callback (10277)
    RECVPROC    m_lpRecvProc;

    // [0x5098] Event callback (10278)
    EVENTPROC   m_lpEventProc;

    // [0x509C] User data pointer (10279)
    void* m_pUserData;

    // [0x50A0] Last Winsock error (10280)
    DWORD       m_LastError;

    // [0x50A4] Receive buffer (20KB) - matches m_RecvBuffer[20480]
    char        m_RecvBuffer[SOCKET_BUFFER_SIZE];

    // [0xA0A4] Error message buffer (128 bytes)
    char        m_szError[128];

    // [0xA124] Show error flag (10313)
    int         m_bShowError;

    // [0xA170] Socket type/mode (10332)
    int         m_SocketType;

    // [0xA174] Critical sections (3x at the end)
    CRITICAL_SECTION m_csRecv;
    CRITICAL_SECTION m_csSend;
    CRITICAL_SECTION m_csEtc;

public:
    // Static members
    static std::map<SOCKET, CWzTcpSock*> m_mapSOCKET_LPOBJ;
    static CRITICAL_SECTION m_critSOCKET_LPOBJ;
    static WNDPROC m_lpOldWndProc;
};
extern CWzTcpSock g_GameServerInfoSock;
#endif // __WZTCPSOCK_H__