#include "StdAfx.h"
#include "IpAddrMng.h"

CIpAddrMng g_CsIpAddrMng;

// Original initial values (CIpAddrMng constructor at 0x411C50).
static const int DEFAULT_MAX_PACKET_PER_SEC = 30;
static const int DEFAULT_MAX_INVALID_PACKETS = 100;
static const DWORD DEFAULT_TIMEOUT_MS = 600000;

CIpAddrMng::CIpAddrMng()
	: m_nMaxPacketPerSec(DEFAULT_MAX_PACKET_PER_SEC)
	, m_nMaxInvalidPackets(DEFAULT_MAX_INVALID_PACKETS)
	, m_dwTimeoutMs(DEFAULT_TIMEOUT_MS)
{
	InitializeCriticalSection(&m_IpAddrMngcs);
}

CIpAddrMng::~CIpAddrMng()
{
	DeleteCriticalSection(&m_IpAddrMngcs);
}

// GetIpAddrInfo returns a copy of the map value; if the address is not yet
// tracked, it is added first. The caller must not hold the lock.
IPADDR_INFO CIpAddrMng::GetIpAddrInfo(unsigned int IP)
{
	IPADDR_INFO Temp;
	memset(&Temp, 0, sizeof(Temp));

	EnterCriticalSection(&m_IpAddrMngcs);

	std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.find(IP);

	if (it == m_mapIPInfo.end())
	{
		AddIpAddr(IP);
		it = m_mapIPInfo.find(IP);
	}

	if (it != m_mapIPInfo.end())
	{
		memcpy(&Temp, &it->second, sizeof(IPADDR_INFO));
	}

	LeaveCriticalSection(&m_IpAddrMngcs);

	return Temp;
}

// AddIpAddr is called with m_IpAddrMngcs already held by the caller.
void CIpAddrMng::AddIpAddr(unsigned int IP)
{
	IPADDR_INFO info;

	DWORD Tick = GetTickCount();

	info.dwCreateTick = Tick;
	info.dwLastActivityTick = Tick;
	info.dwInvalidPacketCount = 0;
	info.dwPacketPerSec = 0;
	info.dwRestricted = 0;

	m_mapIPInfo[IP] = info;
}

// DelIpAddr is called with m_IpAddrMngcs already held by the caller.
void CIpAddrMng::DelIpAddr(unsigned int IP)
{
	std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.find(IP);

	if (it != m_mapIPInfo.end())
	{
		m_mapIPInfo.erase(it);
	}
}

void CIpAddrMng::CheckIpAddrList()
{
	EnterCriticalSection(&m_IpAddrMngcs);

	std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.begin();

	while (it != m_mapIPInfo.end())
	{
		DWORD Timeout = m_dwTimeoutMs;

		if (GetTickCount() - it->second.dwLastActivityTick >= Timeout)
		{
			if (it->second.dwRestricted == 1)
			{
				// The IP octets are taken from the map key (network byte order).
				BYTE* pIP = (BYTE*)&it->first;

				g_LogProc.LogAddFuncColor(1,
					"[DDOS] Release Ip Address : Time Out > %d (IP:%d.%d.%d.%d)",
					Timeout, pIP[0], pIP[1], pIP[2], pIP[3]);

				g_LogFileError.Output("[DDOS] Release Ip Address : Time Out > %d (IP:%d.%d.%d.%d)",
					m_dwTimeoutMs, pIP[0], pIP[1], pIP[2], pIP[3]);
			}
			it = m_mapIPInfo.erase(it);
		}
		else
		{
			++it;
		}
	}

	LeaveCriticalSection(&m_IpAddrMngcs);
}

int CIpAddrMng::IncIpAddrInvalidPacket(unsigned int IP)
{
	int Result = 0;

	EnterCriticalSection(&m_IpAddrMngcs);

	std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.find(IP);

	if (it == m_mapIPInfo.end())
	{
		AddIpAddr(IP);
		it = m_mapIPInfo.find(IP);
	}

	if (it != m_mapIPInfo.end())
	{
		++it->second.dwInvalidPacketCount;
		if (it->second.dwInvalidPacketCount >= (DWORD)m_nMaxInvalidPackets)
		{
			Result = 1;
			it->second.dwRestricted = 1;
		}
	}

	LeaveCriticalSection(&m_IpAddrMngcs);

	return Result;
}

int CIpAddrMng::IncIpAddrPacketPerSec(unsigned int IP)
{
	int Result = 0;

	EnterCriticalSection(&m_IpAddrMngcs);

	std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.find(IP);

	if (it == m_mapIPInfo.end())
	{
		AddIpAddr(IP);
		it = m_mapIPInfo.find(IP);
	}

	if (it != m_mapIPInfo.end())
	{
		IPADDR_INFO& info = it->second;
		++info.dwPacketPerSec;
		if (info.dwPacketPerSec >= (DWORD)m_nMaxPacketPerSec)
		{
			Result = 1;
			info.dwRestricted = 1;
		}

		// NOTE: The one-second window is measured from dwLastActivityTick and
		// the counter is reset only when more than 1000 ms elapsed.
		DWORD Tick = GetTickCount();
		if ((Tick - info.dwLastActivityTick) > 1000)
		{
			info.dwPacketPerSec = 0;
		}
		info.dwLastActivityTick = Tick;
	}

	LeaveCriticalSection(&m_IpAddrMngcs);

	return Result;
}

void CIpAddrMng::ReleaseRestriectedIpAddr()
{
	EnterCriticalSection(&m_IpAddrMngcs);

	for (std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.begin();
		it != m_mapIPInfo.end();
		++it)
	{
		if (it->second.dwRestricted == 1)
		{
			it->second.dwInvalidPacketCount = 0;
			it->second.dwPacketPerSec = 0;
			it->second.dwRestricted = 0;

			BYTE* pIP = (BYTE*)&it->first;

			g_LogProc.LogAddFuncColor(1,
				"[DDOS] Release Ip Address : All (IP:%d.%d.%d.%d)",
				pIP[0], pIP[1], pIP[2], pIP[3]);

			g_LogFileError.Output("[DDOS] Release Ip Address : All (IP:%d.%d.%d.%d)",
				pIP[0], pIP[1], pIP[2], pIP[3]);
		}
	}

	LeaveCriticalSection(&m_IpAddrMngcs);
}

int CIpAddrMng::GetRestrictedIpIpAddrCount()
{
	EnterCriticalSection(&m_IpAddrMngcs);

	int Count = 0;

	for (std::map<DWORD, IPADDR_INFO>::iterator it = m_mapIPInfo.begin();
		it != m_mapIPInfo.end();
		++it)
	{
		if (it->second.dwRestricted == 1)
		{
			++Count;
		}
	}

	if (Count > 100)
	{
		g_LogFileError.Output(
			"[DDOS] Restricted Ip Count : %d",
			Count);
	}

	LeaveCriticalSection(&m_IpAddrMngcs);

	return Count;
}
