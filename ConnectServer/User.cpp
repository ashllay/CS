#include "stdafx.h"
#include "user.h"

// =========================================================================
// Global Variables
// =========================================================================
OBJECTSTRUCT        gObj[MAX_CONNECTOBJECT];
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
int gObjAdd(SOCKET aSocket, in_addr& pAddr)
{
    EnterCriticalSection(&g_UserLock);

    if (g_ObjIndexQ.empty())
    {
        LeaveCriticalSection(&g_UserLock);
        return -1;
    }

    int nIndex = g_ObjIndexQ.top();
    g_ObjIndexQ.pop();

    gObj[nIndex].m_Index = nIndex;
    gObj[nIndex].m_socket = aSocket;
    gObj[nIndex].Connected = 1;

    // Sets gObj[nIndex].Time via the shared helper
    gObjSetTime(nIndex);

    strcpy(gObj[nIndex].m_Ip, inet_ntoa(pAddr));

    gObj[nIndex].m_IpBytes[0] = pAddr.S_un.S_un_b.s_b1;
    gObj[nIndex].m_IpBytes[1] = pAddr.S_un.S_un_b.s_b2;
    gObj[nIndex].m_IpBytes[2] = pAddr.S_un.S_un_b.s_b3;
    gObj[nIndex].m_IpBytes[3] = pAddr.S_un.S_un_b.s_b4;
    gObj[nIndex].m_IpDword = pAddr.S_un.S_addr;

    // Overwrites the value set by gObjSetTime with a fresher tick
    gObj[nIndex].Time = GetTickCount() + CONNECTION_TIMEOUT;

    InterlockedIncrement(&g_iSockConnNum);

    LeaveCriticalSection(&g_UserLock);
    return nIndex;
}

// =========================================================================
// gObjSetTime
// =========================================================================
void gObjSetTime(int nIndex)
{
    EnterCriticalSection(&g_UserLock);

    if (nIndex < 0 || nIndex > MAX_CONNECTOBJECT - 1)
    {
        LeaveCriticalSection(&g_UserLock);
        return;
    }

    if (gObj[nIndex].Connected >= 1)
    {
        gObj[nIndex].Time = GetTickCount() + CONNECTION_TIMEOUT;
    }

    LeaveCriticalSection(&g_UserLock);
}

// =========================================================================
// gObjDel
// =========================================================================
int gObjDel(int nIndex)
{
    if (nIndex < 0 || nIndex > MAX_CONNECTOBJECT - 1)
        return 0;

    EnterCriticalSection(&g_UserLock);

    if (gObj[nIndex].Connected > 0)
    {
        gObj[nIndex].Connected = 0;
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

            linger ling = { 1, 0 };
            setsockopt(s, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));
            shutdown(s, 3); // original passes 3, not SD_BOTH
            closesocket(s);

            gObj[n].Connected = 0;
            g_ObjIndexQ.push(n);
            InterlockedDecrement(&g_iSockConnNum);
        }
    }

    LeaveCriticalSection(&g_UserLock);
}