#pragma once

#ifndef __IPADDRMNG_H__
#define __IPADDRMNG_H__

#include <windows.h>
#include <map>

// Original _IPADDR_INFO is 0x14 (20) bytes; the IP address itself is the map key.
struct IPADDR_INFO
{
    DWORD dwCreateTick;          // +0x00
    DWORD dwLastActivityTick;    // +0x04
    DWORD dwInvalidPacketCount;  // +0x08
    DWORD dwPacketPerSec;        // +0x0C
    DWORD dwRestricted;          // +0x10
};

class CIpAddrMng
{
public:
    CIpAddrMng();
    virtual ~CIpAddrMng();

    IPADDR_INFO GetIpAddrInfo(unsigned int IP);
    void CheckIpAddrList();
    int IncIpAddrInvalidPacket(unsigned int IP);
    int IncIpAddrPacketPerSec(unsigned int IP);
    void ReleaseRestriectedIpAddr();
    int GetRestrictedIpIpAddrCount();

    void SetMaxPacketPerSec(int nMaxPPS) { m_nMaxPacketPerSec = nMaxPPS; }
    void SetMaxInvalidPackets(int nMaxInvalid) { m_nMaxInvalidPackets = nMaxInvalid; }
    void SetTimeoutMs(DWORD dwTimeout) { m_dwTimeoutMs = dwTimeout; }

protected:
    void AddIpAddr(unsigned int IP);
    void DelIpAddr(unsigned int IP);

    // Layout must match original exactly:
    // +0x00: vfptr (compiler-generated, 4 bytes)
    // +0x04: CRITICAL_SECTION (24 bytes on Win32)
    CRITICAL_SECTION m_IpAddrMngcs;           // +0x04, size 0x18
    // +0x1C: std::map<DWORD, IPADDR_INFO>
    // In VS2008, std::map is typically 16 bytes (compressed_pair + nil node pointer)
    std::map<DWORD, IPADDR_INFO> m_mapIPInfo;  // +0x1C

    // +0x2C: Configuration
    int m_nMaxPacketPerSec;          // +0x2C, default 30
    int m_nMaxInvalidPackets;        // +0x30, default 100
    DWORD m_dwTimeoutMs;             // +0x34, default 600000
};

extern CIpAddrMng g_CsIpAddrMng;

#endif
