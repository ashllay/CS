#include "stdafx.h"
#include "WhatsUpDummyServer.h"

// =========================================================================
// Static Members Initialization
// =========================================================================
WNDPROC CWhatsUpDummyServer::m_lpOldProc = NULL;

// =========================================================================
// Constructor
// =========================================================================
CWhatsUpDummyServer::CWhatsUpDummyServer()
    : m_hSocket(INVALID_SOCKET)
    , m_dwReserved(0)
    , m_hNotifyWnd(NULL)
    , m_nPort(0)
{
    // Initialize Winsock (original calls WSAStartup in ctor)
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        // Log error if logging system available
        // Original doesn't log here, so we follow suit
    }
}

// =========================================================================
// Destructor
// =========================================================================
CWhatsUpDummyServer::~CWhatsUpDummyServer()
{
    // Close socket if open
    if (m_hSocket != INVALID_SOCKET)
    {
        closesocket(m_hSocket);
        m_hSocket = INVALID_SOCKET;
    }

    // Note: Winsock cleanup (WSACleanup) is typically done at app shutdown,
    // not in individual object destructors, to avoid premature cleanup
}

// =========================================================================
// Start - Begin Listening for "What's Up" Probes
// Returns: TRUE on success, FALSE on error
// =========================================================================
BOOL CWhatsUpDummyServer::Start(HWND hWnd, unsigned short nPort)
{
    if (!hWnd || !IsWindow(hWnd))
        return FALSE;

    m_hNotifyWnd = hWnd;
    m_nPort = nPort;

    // Create TCP socket (original: socket(2, 1, 0) = AF_INET, SOCK_STREAM, IPPROTO_TCP)
    m_hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_hSocket == INVALID_SOCKET)
    {
        return FALSE;
    }

    // Setup address structure
    sockaddr_in addr = { 0 };
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on all interfaces
    addr.sin_port = htons(nPort);

    // Bind socket to port
    if (bind(m_hSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(m_hSocket);
        m_hSocket = INVALID_SOCKET;
        return FALSE;
    }

    // Start listening (for TCP)
    if (listen(m_hSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        closesocket(m_hSocket);
        m_hSocket = INVALID_SOCKET;
        return FALSE;
    }

    // Setup async notifications
    // Original: 0x40F = WM_WHATSUP_EVENT, 8 = FD_ACCEPT
    if (WSAAsyncSelect(m_hSocket, hWnd, WM_WHATSUP_EVENT, FD_ACCEPT | FD_READ | FD_CLOSE) == SOCKET_ERROR)
    {
        closesocket(m_hSocket);
        m_hSocket = INVALID_SOCKET;
        return FALSE;
    }

    // Subclass window to receive async socket messages
    if (!m_lpOldProc)
    {
        m_lpOldProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(hWnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(ParentWndProc)));
    }

    return TRUE;
}

// =========================================================================
// ParentWndProc - Static: Handle Async Socket Messages
// Called when WM_WHATSUP_EVENT (0x40F) is received
// =========================================================================
LRESULT CALLBACK CWhatsUpDummyServer::ParentWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    // Only handle our custom socket event message
    if (Msg != WM_WHATSUP_EVENT)
    {
        // Chain to original window proc for other messages
        if (m_lpOldProc)
        {
            return CallWindowProcA(m_lpOldProc, hWnd, Msg, wParam, lParam);
        }
        return DefWindowProcA(hWnd, Msg, wParam, lParam);
    }

    // Extract event type and error code from lParam
    unsigned short nEvent = LOWORD(lParam);
    unsigned short nError = HIWORD(lParam);
    SOCKET hSocket = static_cast<SOCKET>(wParam);

    switch (nEvent)
    {
    case FD_READ:  // 1 - Data available to read
    {
        if (nError)
        {
            // Error on read - close socket
            closesocket(hSocket);
            break;
        }

        // Read and discard probe data (original just reads up to 256 bytes)
        char szBuffer[256];
        int nRecv = recv(hSocket, szBuffer, sizeof(szBuffer), 0);
        if (nRecv <= 0)
        {
            closesocket(hSocket);
        }
        // Note: Original doesn't send response - it's a "dummy" server
        // Real implementation might send status packet here
    }
    break;

    case FD_ACCEPT:  // 8 - Incoming connection request
    {
        if (nError)
        {
            // Error on accept - close listening socket
            closesocket(hSocket);
            break;
        }

        // Accept the connection
        sockaddr_in addr = { 0 };
        int nAddrLen = sizeof(addr);

        SOCKET hAccepted = accept(hSocket, reinterpret_cast<sockaddr*>(&addr), &nAddrLen);
        if (hAccepted != INVALID_SOCKET)
        {
            // Setup async notifications for accepted socket
            if (WSAAsyncSelect(hAccepted, hWnd, WM_WHATSUP_EVENT,
                FD_READ | FD_CLOSE) == SOCKET_ERROR)
            {
                closesocket(hAccepted);
            }
            // Note: Accepted sockets are not tracked in original - 
            // they're just read from and closed. No persistent state.
        }
    }
    break;

    case FD_CLOSE:  // 0x20 = 32 - Connection closed
    {
        closesocket(hSocket);
        // Return 0 to indicate message handled
        return 0;
    }
    break;

    default:
        // Unknown event - close socket to be safe
        closesocket(hSocket);
        break;
    }

    // Return 0 for handled messages
    return 0;
}