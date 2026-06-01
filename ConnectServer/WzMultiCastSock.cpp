#include "stdafx.h"
#include "WzMultiCastSock.h"

CWzMultiCastSock::CWzMultiCastSock()
{
    WSADATA WSAData;

    WSAStartup(MAKEWORD(1, 1), &WSAData);

    m_hSocket = INVALID_SOCKET;

    m_nEnumCount = 0;

    m_pfnRecvCallback = NULL;
    m_pfnErrorCallback = NULL;

    memset(m_szLocalIP, 0, 0x280);
}

CWzMultiCastSock::~CWzMultiCastSock()
{
    shutdown(m_hSocket, 0);
    closesocket(m_hSocket);

    WSACleanup();
}

int CWzMultiCastSock::DoEnumIpAddress()
{
    hostent* pHost;
    in_addr** h_addr_list;
    in_addr* pAddr;
    char name[128];

    m_nEnumCount = 0;

    memset(m_EnumIPs, 0, sizeof(m_EnumIPs));

    gethostname(name, sizeof(name));

    pHost = gethostbyname(name);

    if (pHost->h_addrtype == AF_INET)
    {
        h_addr_list = (in_addr**)pHost->h_addr_list;

        pAddr = *h_addr_list;

        pHost->h_addr_list = (char**)(h_addr_list + 1);

        while (pAddr)
        {
            m_EnumIPs[m_nEnumCount].bValid = 1;

            strcpy(
                m_EnumIPs[m_nEnumCount].szIP,
                inet_ntoa(*pAddr));

            ++m_nEnumCount;

            if (m_nEnumCount >= MAX_ENUM_IPS)
                break;

            h_addr_list = (in_addr**)pHost->h_addr_list;

            pAddr = *h_addr_list;

            pHost->h_addr_list = (char**)(h_addr_list + 1);
        }
    }

    return m_nEnumCount;
}

int CWzMultiCastSock::GetEnummedIpAddress(
    unsigned int nIndex,
    char* szOutBuffer)
{
    DWORD* pSrc;

    if (nIndex >= MAX_ENUM_IPS)
        return 0;

    if (m_EnumIPs[nIndex].bValid != 1)
        return 0;

    if (!szOutBuffer)
        return 0;

    pSrc = (DWORD*)m_EnumIPs[nIndex].szIP;

    ((DWORD*)szOutBuffer)[0] = pSrc[0];
    ((DWORD*)szOutBuffer)[1] = pSrc[1];
    ((DWORD*)szOutBuffer)[2] = pSrc[2];
    ((DWORD*)szOutBuffer)[3] = pSrc[3];

    return 1;
}

int CWzMultiCastSock::StartMultiCast(char* szLocalIP)
{
    sockaddr name;
    ip_mreq mreq;
    DWORD dwInterface;

    if (m_hSocket != INVALID_SOCKET)
    {
        closesocket(m_hSocket);
        m_hSocket = INVALID_SOCKET;
    }

    if (szLocalIP)
    {
        memcpy(m_szLocalIP, szLocalIP, 16);
    }
    else
    {
        char szTempIP[16];

        if (DoEnumIpAddress() <= 0)
            return 0;

        if (!GetEnummedIpAddress(0, szTempIP))
            return 0;

        memcpy(m_szLocalIP, szTempIP, 16);
    }

    m_szLocalIP[15] = 0;

    m_hSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (m_hSocket == INVALID_SOCKET)
    {
        m_hSocket = INVALID_SOCKET;
        return 0;
    }

    memset(&name, 0, sizeof(name));

    name.sa_family = AF_INET;

    *(WORD*)name.sa_data = htons(DEFAULT_MULTICAST_PORT);

    *(DWORD*)&name.sa_data[2] = htonl(INADDR_ANY);

    if (bind(m_hSocket, &name, 16) == SOCKET_ERROR)
    {
        SaveLastError(0);

        closesocket(m_hSocket);

        m_hSocket = INVALID_SOCKET;

        return 0;
    }

    mreq.imr_multiaddr.s_addr =
        inet_addr(DEFAULT_MULTICAST_GROUP);

    mreq.imr_interface.s_addr =
        inet_addr(m_szLocalIP);

    if (setsockopt(
        m_hSocket,
        0,
        5,
        (char*)&mreq,
        8) == SOCKET_ERROR)
    {
        SaveLastError(0);

        closesocket(m_hSocket);

        m_hSocket = INVALID_SOCKET;

        return 0;
    }

    dwInterface = inet_addr(m_szLocalIP);

    if (setsockopt(
        m_hSocket,
        0,
        2,
        (char*)&dwInterface,
        4) == SOCKET_ERROR)
    {
        SaveLastError(0);

        closesocket(m_hSocket);

        m_hSocket = INVALID_SOCKET;

        return 0;
    }

    m_hRecvThread = (HANDLE)_beginthreadex(
        NULL,
        0,
        RecvThreadProc,
        this,
        0,
        (unsigned int*)&m_dwRecvThreadID);

    return 1;
}

int CWzMultiCastSock::ReStartMultiCast()
{
    EndMultiCast();

    return StartMultiCast(m_szLocalIP);
}

void CWzMultiCastSock::EndMultiCast()
{
    if (m_hSocket != INVALID_SOCKET)
    {
        closesocket(m_hSocket);

        m_hSocket = INVALID_SOCKET;
    }

    if (m_hRecvThread != INVALID_HANDLE_VALUE)
    {
        TerminateThread(m_hRecvThread, 0);

        CloseHandle(m_hRecvThread);

        m_hRecvThread = INVALID_HANDLE_VALUE;
    }
}

int CWzMultiCastSock::SendData(char* buf, int len)
{
    sockaddr to;

    if (!buf)
        return 0;

    to.sa_family = AF_INET;
    *(DWORD*)&to.sa_data[2] =inet_addr(DEFAULT_MULTICAST_GROUP);
    *(WORD*)to.sa_data =htons(DEFAULT_MULTICAST_PORT);

    if (sendto(m_hSocket,buf,len,0,&to,16) >= 0)
    {
        return 1;
    }
    SaveLastError(0);
    closesocket(m_hSocket);
    m_hSocket = INVALID_SOCKET;

    return 0;
}

void CWzMultiCastSock::SaveLastError(int nError)
{
    int Error;
    Error = nError;

    if (!Error)
        Error = WSAGetLastError();

    m_nLastError = Error;

    memset(m_szErrorMsg, 0, sizeof(m_szErrorMsg));

    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM,NULL,m_nLastError,0,m_szErrorMsg,sizeof(m_szErrorMsg),NULL);

    if (m_pfnErrorCallback)
    {
        m_pfnErrorCallback(this,m_nLastError,m_szErrorMsg);
    }
}

void CWzMultiCastSock::SavePrivateErrorMessage(char* szMsg)
{
    m_nLastError = 0;

    strcpy(m_szErrorMsg, szMsg);

    if (m_pfnErrorCallback)
    {
        m_pfnErrorCallback(this,m_nLastError,m_szErrorMsg);
    }
}

unsigned int __stdcall CWzMultiCastSock::RecvThreadProc(void* pParam)
{
    CWzMultiCastSock* lpThis;
    sockaddr from;
    int fromlen;
    int nRecv;

    char buf[MULTICAST_RECV_BUFFER_SIZE];

    lpThis = (CWzMultiCastSock*)pParam;

    fromlen = 16;

    while (1)
    {
        nRecv = recvfrom(lpThis->m_hSocket,buf,MULTICAST_RECV_BUFFER_SIZE,0,&from,&fromlen);

        if (nRecv == SOCKET_ERROR)
        {
            lpThis->SaveLastError(0);
            closesocket(lpThis->m_hSocket);
            goto LABEL_ERROR;
        }

        if (nRecv < 0)
            break;

        if ((BYTE)buf[0] == 0xC1)
        {
            if (lpThis->m_pfnRecvCallback)
            {
                lpThis->m_pfnRecvCallback(lpThis,*(DWORD*)&from.sa_data[2],(BYTE)buf[2],(BYTE)buf[3],buf,nRecv);
            }
        }
        else if ((BYTE)buf[0] == 0xC2)
        {
            if (lpThis->m_pfnRecvCallback)
            {
                lpThis->m_pfnRecvCallback(lpThis,*(DWORD*)&from.sa_data[2],(BYTE)buf[3],(BYTE)buf[4],buf,
                    nRecv);
            }
        }
    }

    lpThis->SavePrivateErrorMessage("RecvThreadProc error");
    closesocket(lpThis->m_hSocket);

LABEL_ERROR:
    lpThis->m_hSocket = INVALID_SOCKET;
    return 0;
}