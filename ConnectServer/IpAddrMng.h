#pragma once

#ifndef __IPADDRMNG_H__
#define __IPADDRMNG_H__

#include <windows.h>
#include <map>

// IP address info structure (matches original _IPADDR_INFO layout exactly)
// Size: 0x28 (40 bytes)
struct IPADDR_INFO
{
    DWORD dwIP;              // +0x00
    DWORD dwUnknown04;       // +0x04
    DWORD dwLastActivity;    // +0x08
    BYTE bIP[4];            // +0x0C
    DWORD dwUnknown10;       // +0x10
    DWORD dwLastPPSReset;    // +0x14
    DWORD dwInvalidPacketCount; // +0x18
    DWORD dwPacketPerSec;    // +0x1C
    DWORD dwRestrictFlags;   // +0x20
    DWORD dwUnknown24;       // +0x24
};

class CIpAddrMng
{
public:
    CIpAddrMng();
    virtual ~CIpAddrMng();

    void* GetIpAddrInfo(void* pOutBuffer, DWORD* pIP);
    void AddIpAddr(DWORD* pIP);
    void DelIpAddr(DWORD dwIP);
    void CheckIpAddrList();
    int IncIpAddrInvalidPacket(DWORD* pIP);
    int IncIpAddrPacketPerSec(DWORD* pIP);
    void ReleaseRestriectedIpAddr();
    int GetRestrictedIpIpAddrCount();

    void SetMaxPacketPerSec(int nMaxPPS) { m_nMaxPacketPerSec = nMaxPPS; }
    void SetMaxInvalidPackets(int nMaxInvalid) { m_nMaxInvalidPackets = nMaxInvalid; }
    void SetTimeoutMs(DWORD dwTimeout) { m_dwTimeoutMs = dwTimeout; }

private:
    // Layout must match original exactly:
    // +0x00: vfptr (compiler-generated, 4 bytes)
    // +0x04: CRITICAL_SECTION (24 bytes on Win32)
    CRITICAL_SECTION m_cs;           // +0x04, size 0x18

    // +0x1C: std::map<DWORD, IPADDR_INFO> 
    // In VS2008, std::map is typically 16 bytes (compressed_pair + nil node pointer)
    std::map<DWORD, IPADDR_INFO> m_mapIPInfo;  // +0x1C

    // +0x2C: Configuration
    int m_nMaxPacketPerSec;          // +0x2C, default 30
    int m_nMaxInvalidPackets;        // +0x30, default 100
    DWORD m_dwTimeoutMs;             // +0x34, default 600000
};

extern CIpAddrMng g_CsIpAddrMng;

#endif // __IPADDRMNG_H__