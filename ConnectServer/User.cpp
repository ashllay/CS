#include "stdafx.h"
#include "user.h"

// =========================================================================
// Global Variables
// =========================================================================
OBJECTSTRUCT        gObj[MAX_CONNECTOBJECT];
int                 gObjConnected[MAX_CONNECTOBJECT];
DWORD               gObjTimeout[MAX_CONNECTOBJECT];
CRITICAL_SECTION    g_UserLock;
std::stack<int>     g_ObjIndexQ;        // Original uses std::stack<int>

// =========================================================================
// gObjInit
// =========================================================================
void gObjInit()
{
    InitializeCriticalSection(&g_UserLock);

    for (int i = 0; i < MAX_CONNECTOBJECT; ++i)
    {
        EnterCriticalSection(&g_UserLock);
        g_ObjIndexQ.push(i);
        LeaveCriticalSection(&g_UserLock);
    }
}

// =========================================================================
// gObjEnd
// =========================================================================
void gObjEnd()
{
    DeleteCriticalSection(&g_UserLock);
}

// =========================================================================
// gObjAdd
// =========================================================================
int gObjAdd(SOCKET aSocket, struct in_addr* pAddr)
{
    if (!pAddr)
        return -1;

    EnterCriticalSection(&g_UserLock);

    if (g_ObjIndexQ.empty())
    {
        LeaveCriticalSection(&g_UserLock);
        return -1;
    }

    int nIndex = g_ObjIndexQ.top();
    g_ObjIndexQ.pop();

    // DON'T memset the whole object — the constructor already initialized m_Lock
    // Just set the connection fields
    gObjConnected[nIndex] = 1;
    gObj[nIndex].m_socket = aSocket;

    // Store IP
    char* szIP = inet_ntoa(*pAddr);
    strcpy(gObj[nIndex].m_Ip, szIP);

    // Store IP bytes
    gObj[nIndex].m_IpBytes[0] = pAddr->S_un.S_un_b.s_b1;
    gObj[nIndex].m_IpBytes[1] = pAddr->S_un.S_un_b.s_b2;
    gObj[nIndex].m_IpBytes[2] = pAddr->S_un.S_un_b.s_b3;
    gObj[nIndex].m_IpBytes[3] = pAddr->S_un.S_un_b.s_b4;
    gObj[nIndex].m_IpDword = pAddr->S_un.S_addr;

    gObjTimeout[nIndex] = GetTickCount() + CONNECTION_TIMEOUT;

    InterlockedIncrement(&g_iSockConnNum);

    LeaveCriticalSection(&g_UserLock);
    return nIndex;
}

// =========================================================================
// gObjDel
// =========================================================================
int gObjDel(int nIndex)
{
    if (nIndex >= MAX_CONNECTOBJECT)
        return 0;

    EnterCriticalSection(&g_UserLock);

    if (gObjConnected[nIndex] > 0)
    {
        gObjConnected[nIndex] = 0;
        g_ObjIndexQ.push(nIndex);
        InterlockedDecrement(&g_iSockConnNum);
    }

    LeaveCriticalSection(&g_UserLock);
    return 1;
}

// =========================================================================
// gObjSetTime
// =========================================================================
void gObjSetTime(int nIndex)
{
    if (nIndex >= MAX_CONNECTOBJECT)
        return;

    EnterCriticalSection(&g_UserLock);

    if (gObjConnected[nIndex] >= 1)
        gObjTimeout[nIndex] = GetTickCount() + CONNECTION_TIMEOUT;

    LeaveCriticalSection(&g_UserLock);
}

// =========================================================================
// gObjGetSocket
// =========================================================================
int gObjGetSocket(SOCKET socket)
{
    for (int n = 0; n < MAX_CONNECTOBJECT; ++n)
    {
        if (gObjConnected[n] && gObj[n].m_socket == socket)
            return n;
    }
    return -1;
}

// =========================================================================
// gObjCheckTimeOut
// =========================================================================
void gObjCheckTimeOut()
{
    EnterCriticalSection(&g_UserLock);

    DWORD TickCount = GetTickCount();

    for (int n = 0; n < MAX_CONNECTOBJECT; ++n)
    {
        if (gObjConnected[n] > 0 && gObjTimeout[n] < TickCount)
        {
            SOCKET s = gObj[n].m_socket;

            BOOL bDontLinger = TRUE;
            setsockopt(s, SOL_SOCKET, SO_DONTLINGER, (char*)&bDontLinger, sizeof(bDontLinger));
            shutdown(s, SD_BOTH);
            closesocket(s);

            gObjConnected[n] = 0;
            g_ObjIndexQ.push(n);
            InterlockedDecrement(&g_iSockConnNum);
        }
    }

    LeaveCriticalSection(&g_UserLock);
}