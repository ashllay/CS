#pragma once

#ifndef __USER_H__
#define __USER_H__

#include <windows.h>
#include <winsock2.h>
#include <stack>
#include "giocp.h"

#define MAX_CONNECTOBJECT   10000             // 0x2710
#define CONNECTION_TIMEOUT  180000            // 3 minutes (in ms)

typedef struct _OBJECTSTRUCT
{
    // The constructor only initializes m_Lock at offset 24724
    // All other fields are set by gObjAdd

    int                 m_Index;              // +0x0000
    BOOL                Connected;            // +0x0004
    SOCKET              m_socket;             // +0x0008
    char                m_Ip[16];             // +0x000C
    DWORD               Time;                 // +0x001C
    BYTE                m_IpBytes[4];         // +0x0020
    DWORD               m_IpDword;            // +0x0024
    PER_SOCKET_CONTEXT  PerSocketContext;     // +0x0028
    CCriticalSec        m_Lock;               // +0x6094 (24724)

    _OBJECTSTRUCT()
    {
        new (&m_Lock) CCriticalSec();
    }

    ~_OBJECTSTRUCT()
    {
        m_Lock.~CCriticalSec();
    }
} OBJECTSTRUCT;

// Global connection pool
extern OBJECTSTRUCT     gObj[MAX_CONNECTOBJECT];
extern int              gObjConnected[MAX_CONNECTOBJECT];
extern DWORD            gObjTimeout[MAX_CONNECTOBJECT];
extern CRITICAL_SECTION g_UserLock;
extern std::stack<int>  g_ObjIndexQ;          // Original uses std::stack<int>

// Public API
void        gObjInit();
void        gObjEnd();
int         gObjAdd(SOCKET aSocket, in_addr* pAddr);
int         gObjDel(int nIndex);
int         gObjGetSocket(SOCKET socket);
void        gObjSetTime(int nIndex);
void        gObjCheckTimeOut();

#endif // __USER_H__