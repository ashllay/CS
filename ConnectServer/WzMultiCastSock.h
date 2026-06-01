#pragma once

#ifndef __WZMULTICASTSOCK_H__
#define __WZMULTICASTSOCK_H__

#include <winsock2.h>
#include <windows.h>
#include <process.h>

#pragma comment(lib, "ws2_32.lib")

#define DEFAULT_MULTICAST_GROUP    "224.0.144.137"
#define DEFAULT_MULTICAST_PORT     0x9389
#define MULTICAST_RECV_BUFFER_SIZE 24588
#define MAX_ENUM_IPS               32

// Packet type codes (matches Mu Online protocol)
enum MULTICAST_PACKET_TYPE
{
    MULTICAST_PKT_CONNECT_SERVER_INFO = 1,  // 0xC1 header
    MULTICAST_PKT_CCS_SHARE_INFO = 3         // 0xC2 header
};

struct EnumIPEntry
{
    int  bValid;
    char szIP[16];
};

class CWzMultiCastSock
{
public:
    CWzMultiCastSock();
    virtual ~CWzMultiCastSock();

public:
    int  DoEnumIpAddress();
    int  GetEnummedIpAddress(unsigned int nIndex, char* szOutBuffer);

    int  StartMultiCast(char* szLocalIP = NULL);
    int  ReStartMultiCast();
    void EndMultiCast();

    int  SendData(char* buf, int len);

    void SaveLastError(int nError = 0);
    void SavePrivateErrorMessage(char* szMsg);

    static unsigned int __stdcall RecvThreadProc(void* pParam);

public:
    // +0x000
    SOCKET m_hSocket;

    // +0x004
    char m_szLocalIP[16];

    // +0x014
    EnumIPEntry m_EnumIPs[MAX_ENUM_IPS];

    // +0x294
    int m_nEnumCount;

    // +0x298
    int m_nLastError;

    // +0x29C
    char m_szErrorMsg[260];

    // +0x3A0
    HANDLE m_hRecvThread;

    // +0x3A4
    DWORD m_dwRecvThreadID;

    // +0x3A8
    void(__cdecl* m_pfnRecvCallback)(CWzMultiCastSock*,int,int,int,char*,int);
    // +0x3AC
    void(__cdecl* m_pfnErrorCallback)(CWzMultiCastSock*,int,char*);
};

#endif