#include "StdAfx.h"
#include "IpAddrMng.h"
#include "LogProc.h"
#include "LogToFile.h"

//CIpAddrMng g_CsIpAddrMng;

static const int DEFAULT_MAX_PACKET_PER_SEC = 30;
static const int DEFAULT_MAX_INVALID_PACKETS = 100;
static const DWORD DEFAULT_TIMEOUT_MS = 600000;

CIpAddrMng::CIpAddrMng()
    : m_nMaxPacketPerSec(DEFAULT_MAX_PACKET_PER_SEC)
    , m_nMaxInvalidPackets(DEFAULT_MAX_INVALID_PACKETS)
    , m_dwTimeoutMs(DEFAULT_TIMEOUT_MS)
{
    InitializeCriticalSection(&m_cs);
}

CIpAddrMng::~CIpAddrMng()
{
    DeleteCriticalSection(&m_cs);
}

void* CIpAddrMng::GetIpAddrInfo(void* pOutBuffer, DWORD* pIP)
{
    if (!pOutBuffer || !pIP)
        return NULL;

    ZeroMemory(pOutBuffer, 0x14);

    EnterCriticalSection(&m_cs);

    std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.find(*pIP);

    if (it != m_mapIPInfo.end())
    {
        memcpy(pOutBuffer, &it->second, 0x14);
    }
    else
    {
        AddIpAddr(pIP);
        it = m_mapIPInfo.find(*pIP);
        if (it != m_mapIPInfo.end())
        {
            memcpy(pOutBuffer, &it->second, 0x14);
        }
    }

    LeaveCriticalSection(&m_cs);

    return pOutBuffer;
}

void CIpAddrMng::AddIpAddr(DWORD* pIP)
{
    if (!pIP)
        return;

    DWORD dwNow = GetTickCount();

    IPADDR_INFO info;
    ZeroMemory(&info, sizeof(info));

    info.dwIP = *pIP;
    info.dwUnknown04 = dwNow;
    info.dwLastActivity = dwNow;

    info.bIP[0] = (*pIP >> 24) & 0xFF;
    info.bIP[1] = (*pIP >> 16) & 0xFF;
    info.bIP[2] = (*pIP >> 8) & 0xFF;
    info.bIP[3] = *pIP & 0xFF;

    info.dwLastPPSReset = dwNow;
    // other fields remain 0

    m_mapIPInfo[*pIP] = info;
}

void CIpAddrMng::DelIpAddr(DWORD dwIP)
{
    EnterCriticalSection(&m_cs);
    m_mapIPInfo.erase(dwIP);
    LeaveCriticalSection(&m_cs);
}

void CIpAddrMng::CheckIpAddrList()
{
    EnterCriticalSection(&m_cs);

    DWORD dwNow = GetTickCount();

    std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.begin();
    while (it != m_mapIPInfo.end())
    {
        IPADDR_INFO& info = it->second;

        if (dwNow - info.dwLastActivity >= m_dwTimeoutMs)
        {
            if (info.dwRestrictFlags == 1)
            {
                g_LogProc.LogAddFuncColor(LOG_COLOR_NORMAL,
                    "[DDOS] Release Ip Address : Time Out > %d (IP:%d.%d.%d.%d)",
                    m_dwTimeoutMs,
                    info.bIP[0], info.bIP[1], info.bIP[2], info.bIP[3]);

                g_LogFileError.Output(
                    "[DDOS] Release Ip Address : Time Out > %d (IP:%d.%d.%d.%d)",
                    m_dwTimeoutMs,
                    info.bIP[0], info.bIP[1], info.bIP[2], info.bIP[3]);
            }

            m_mapIPInfo.erase(it++);
        }
        else
        {
            ++it;
        }
    }

    LeaveCriticalSection(&m_cs);
}

int CIpAddrMng::IncIpAddrInvalidPacket(DWORD* pIP)
{
    if (!pIP)
        return 0;

    int bRestricted = 0;

    EnterCriticalSection(&m_cs);

    std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.find(*pIP);

    if (it == m_mapIPInfo.end())
    {
        AddIpAddr(pIP);
        it = m_mapIPInfo.find(*pIP);
    }

    if (it != m_mapIPInfo.end())
    {
        IPADDR_INFO& info = it->second;
        ++info.dwInvalidPacketCount;

        if (info.dwInvalidPacketCount >= static_cast<DWORD>(m_nMaxInvalidPackets))
        {
            bRestricted = 1;
            info.dwRestrictFlags = 1;
        }
    }

    LeaveCriticalSection(&m_cs);

    return bRestricted;
}

int CIpAddrMng::IncIpAddrPacketPerSec(DWORD* pIP)
{
    if (!pIP)
        return 0;

    int bRestricted = 0;
    DWORD dwNow = GetTickCount();

    EnterCriticalSection(&m_cs);

    std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.find(*pIP);

    if (it != m_mapIPInfo.end())
    {
        IPADDR_INFO& info = it->second;

        if (dwNow - info.dwLastPPSReset > 1000)
        {
            info.dwPacketPerSec = 0;
        }

        ++info.dwPacketPerSec;

        if (info.dwPacketPerSec >= static_cast<DWORD>(m_nMaxPacketPerSec))
        {
            bRestricted = 1;
            info.dwRestrictFlags = 1;
        }

        info.dwLastActivity = dwNow;
    }
    else
    {
        AddIpAddr(pIP);
        it = m_mapIPInfo.find(*pIP);

        if (it != m_mapIPInfo.end())
        {
            IPADDR_INFO& info = it->second;

            if (dwNow - info.dwLastPPSReset > 1000)
            {
                info.dwPacketPerSec = 0;
            }

            ++info.dwPacketPerSec;

            if (info.dwPacketPerSec >= static_cast<DWORD>(m_nMaxPacketPerSec))
            {
                bRestricted = 1;
                info.dwRestrictFlags = 1;
            }

            info.dwLastActivity = dwNow;
        }
    }

    LeaveCriticalSection(&m_cs);

    return bRestricted;
}

void CIpAddrMng::ReleaseRestriectedIpAddr()
{
    EnterCriticalSection(&m_cs);

    for (std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.begin();
        it != m_mapIPInfo.end();
        ++it)
    {
        IPADDR_INFO& info = it->second;

        if (info.dwRestrictFlags == 1)
        {
            info.dwInvalidPacketCount = 0;
            info.dwPacketPerSec = 0;
            info.dwRestrictFlags = 0;

            g_LogProc.LogAddFuncColor(LOG_COLOR_NORMAL,
                "[DDOS] Release Ip Address : All (IP:%d.%d.%d.%d)",
                info.bIP[0], info.bIP[1], info.bIP[2], info.bIP[3]);

            g_LogFileError.Output(
                "[DDOS] Release Ip Address : All (IP:%d.%d.%d.%d)",
                info.bIP[0], info.bIP[1], info.bIP[2], info.bIP[3]);
        }
    }

    LeaveCriticalSection(&m_cs);
}

int CIpAddrMng::GetRestrictedIpIpAddrCount()
{
    EnterCriticalSection(&m_cs);

    int nCount = 0;

    for (std::map<DWORD, IPADDR_INFO>::const_iterator it = m_mapIPInfo.begin();
        it != m_mapIPInfo.end();
        ++it)
    {
        if (it->second.dwRestrictFlags == 1)
        {
            ++nCount;
        }
    }

    if (nCount > 100)
    {
        g_LogFileError.Output("[DDOS] Restricted Ip Count : %d", nCount);
    }

    LeaveCriticalSection(&m_cs);

    return nCount;
}