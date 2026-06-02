#include "stdafx.h"
#include "user.h"

// =========================================================================
// Global Variables
// =========================================================================
OBJECTSTRUCT        gObj[MAX_CONNECTOBJECT];
int                 gObjConnected[MAX_CONNECTOBJECT];  // Points to gObj[n].Connected
DWORD               gObjTimeout[MAX_CONNECTOBJECT];    // Points to gObj[n].Time
CRITICAL_SECTION    g_UserLock;
std::stack<int>     g_ObjIndexQ;

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
int gObjAdd(SOCKET aSocket, in_addr* pAddr)
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

    // The original sets m_Index first
    gObj[nIndex].m_Index = nIndex;
    gObj[nIndex].m_socket = aSocket;

    // Set Connected flag (int, not BOOL)
    gObj[nIndex].Connected = 1;

    // Set Time immediately
    gObj[nIndex].Time = GetTickCount() + CONNECTION_TIMEOUT;

    // Store IP string
    char* szIP = inet_ntoa(*pAddr);
    strcpy(gObj[nIndex].m_Ip, szIP);

    // Store IP bytes
    gObj[nIndex].m_IpBytes[0] = pAddr->S_un.S_un_b.s_b1;
    gObj[nIndex].m_IpBytes[1] = pAddr->S_un.S_un_b.s_b2;
    gObj[nIndex].m_IpBytes[2] = pAddr->S_un.S_un_b.s_b3;
    gObj[nIndex].m_IpBytes[3] = pAddr->S_un.S_un_b.s_b4;
    gObj[nIndex].m_IpDword = pAddr->S_un.S_addr;

    // Sync separate arrays with structure
    gObjConnected[nIndex] = 1;
    gObjTimeout[nIndex] = gObj[nIndex].Time;

    InterlockedIncrement(&g_iSockConnNum);

    LeaveCriticalSection(&g_UserLock);
    return nIndex;
}

// =========================================================================
// gObjSetTime
// =========================================================================
void gObjSetTime(int nIndex)
{
    if (nIndex >= MAX_CONNECTOBJECT)
        return;

    EnterCriticalSection(&g_UserLock);

    // Check using int comparison (original uses >= 1)
    if (gObj[nIndex].Connected >= 1)
    {
        DWORD dwTime = GetTickCount() + CONNECTION_TIMEOUT;
        gObj[nIndex].Time = dwTime;
        gObjTimeout[nIndex] = dwTime;
    }

    LeaveCriticalSection(&g_UserLock);
}

// =========================================================================
// gObjDel
// =========================================================================
int gObjDel(int nIndex)
{
    if (nIndex >= MAX_CONNECTOBJECT)
        return 0;

    EnterCriticalSection(&g_UserLock);

    // Check > 0 (not >= 1) matching original logic
    if (gObj[nIndex].Connected > 0)
    {
        gObj[nIndex].Connected = 0;
        gObjConnected[nIndex] = 0;
        g_ObjIndexQ.push(nIndex);
        InterlockedDecrement(&g_iSockConnNum);
    }

    LeaveCriticalSection(&g_UserLock);
    return 1;
}

// =========================================================================
// gObjGetSocket
// =========================================================================
int gObjGetSocket(SOCKET socket)
{
    // Iterate through objects directly like original
    for (int n = 0; n < MAX_CONNECTOBJECT; ++n)
    {
        if (gObj[n].Connected && gObj[n].m_socket == socket)
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
        if (gObj[n].Connected > 0 && gObj[n].Time < TickCount)
        {
            SOCKET s = gObj[n].m_socket;

            // SO_DONTLINGER = 0xFFFF, SO_LINGER = 128 (0x80)? 
            // Actually setsockopt with level=0xFFFF (SOL_SOCKET), optname=128 (SO_LINGER)
            struct linger {
                u_short l_onoff;
                u_short l_linger;
            } ling = { 1, 0 };
            setsockopt(s, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));
            shutdown(s, SD_BOTH);
            closesocket(s);

            gObj[n].Connected = 0;
            gObjConnected[n] = 0;
            g_ObjIndexQ.push(n);
            InterlockedDecrement(&g_iSockConnNum);
        }
    }

    LeaveCriticalSection(&g_UserLock);
}