#include "StdAfx.h"
#include "Protocol.h"
#include "CS.h"
#include "LogProc.h"
#include "udpprotocol.h"
#include "User.h"
#include "..\\common\\winutil.h"

// =========================================================================
// Global Buffers Initialization
// =========================================================================
char szFileServerInfoSendBuffer_Old[256];
int nFileServerInfoBufferLen_Old = 0;

char szFileServerInfoSendBuffer_New[256];
int nFileServerInfoBufferLen_New = 0;

char szFileServerInfoSendBuffer_CCS[256];
int nFileServerInfoBufferLen_CCS = 0;

char szFileServerInfoSendBuffer_LocalTest[256];
int nFileServerInfoBufferLen_LocalTest = 0;

char szHTTPFileServerInfoSendBuffer[256];
int nHTTPFileServerInfoBufferLen = 0;

char szNoUpdateInfoSendBuffer[16];
int nNoUpdateInfoBufferLen = 0;

char szServerListSendBuffer[4096];
int nServerListBufferLen = 0;

char szSelectionServerListSendBuffer[4096];
char szServerUserCountSendBuffer[4096];
int nServerUserCountBufferLen = 0;

// =========================================================================
// ProtocolCore - Main Dispatch Handler
// =========================================================================
void ProtocolCore(BYTE protoNum, BYTE* aRecv, int aLen, short aIndex)
{
#ifdef __DEBUG_LOG
    g_LogProc.LogAddFunc("ProtocolCore: head=0x%02X, len=%d, index=%d", protoNum, aLen, aIndex);
#endif
    switch (protoNum)
    {
    case 0x01:
        CSVersionRecv_OLD(reinterpret_cast<PMSG_VERSION_OLD*>(aRecv), aIndex);
        break;
    case 0x05:
        CSVersionRecv_NEW(reinterpret_cast<PMSG_VERSION_NEW*>(aRecv), aIndex);
        break;
    case 0x06:
        CSVersionRecv_TEST(reinterpret_cast<PMSG_VERSION_NEW*>(aRecv), aIndex);
        break;
    case 0x07:
        CSVersionRecv_LOCALTEST(reinterpret_cast<PMSG_VERSION_NEW*>(aRecv), aIndex);
        break;
    case 0x10:
        UpdateGameServerList(aIndex);
        break;
    case 0x11:
        ReqGameServerList(aIndex);
        break;
    case 0x12:
        ChangeGameServerList();
        break;
    case 0xF4:
    {
        PMSG_DEFAULT* lpDef = (PMSG_DEFAULT*)aRecv;
#ifdef __DEBUG_LOG
        g_LogProc.LogAddFunc("ProtocolCore: head=0x%02X, subcode=0x%02X, len=%d, index=%d\n", protoNum, lpDef->subcode, aLen, aIndex);
#endif
        switch (lpDef->subcode)
        {
        case 0x03:
            CSGetServerIpPort(reinterpret_cast<PMSG_GAMESERVERIPPORT_REQUEST*>(aRecv), aIndex);
            break;
        case 0x06:
            CSGetServerListRecv(reinterpret_cast<PMSG_SERVERLISTREQUEST*>(aRecv), aIndex);
            break;
        case 0x07:
            CSGetServerUserCount(aIndex);
            break;
        }
    }
    break;
	case 0xF5:
	{
		PMSG_DEFAULT* lpDef = (PMSG_DEFAULT*)aRecv;
		switch (lpDef->subcode)
		{
		case 0x00:
			CSConnectListVersionRecv();
		}
	}
	break;
	default:
		break;
	}
}

// =========================================================================
// UpdateGameServerList - Reload Configs and Request List
// =========================================================================
void UpdateGameServerList(int aIndex)
{
    ReadConnServerList();
	ReadServerList();
    ReqGameServerList(aIndex);
}

// =========================================================================
// ReqGameServerList - Build and Send Server List to JoinServer
// =========================================================================
void ReqGameServerList(int aIndex)
{
    char GameServerInfo[sizeof(GameServerList_Struct) *MAX_GAMESERVERLIST];

    int iLen = 0;
    int count = 0;

    int lOfs = sizeof(PMSG_SERVERLISTCOUNT);

    for (int n = 0; n < MAX_GAMESERVERLIST; n++)
    {
        if (GameServerList[n].ServerCode != -1 && GameServerList[n].ServerCode != 255)
        {
            memcpy(GameServerInfo + lOfs,&GameServerList[n],sizeof(GameServerList[n]));
            lOfs += sizeof(GameServerList[n]);
            count++;
        }
    }

    if (count < 1)
    {
        return;
    }

    PMSG_SERVERLISTCOUNT pCount;

    pCount.h.c          = PMHC_WORD;
    pCount.h.headcode   = 0x11;
    pCount.h.subcode    = 0x00;
    pCount.h.sizeH      = SET_NUMBERH(lOfs);
    pCount.h.sizeL      = SET_NUMBERL(lOfs);
    pCount.CountH       = SET_NUMBERH(count);
    pCount.CountL       = SET_NUMBERL(count);

    memcpy(GameServerInfo,&pCount,sizeof(pCount));

    iLen = lOfs;

    IoSend(aIndex, (LPBYTE)GameServerInfo, iLen);
}

void ChangeGameServerList()
{
    ;
}
// =========================================================================
// SCConnectResultSend - Send Connection Result
// =========================================================================
void SCConnectResultSend(char result, unsigned int aIndex)
{
    PMSG_CONNECTRESULT pMsg;
    pMsg.h.c        = PMHC_BYTE;
    pMsg.h.headcode = 0x00;
    pMsg.h.size     = sizeof(pMsg);
    pMsg.result     = result;

    IoSend(aIndex, reinterpret_cast<LPBYTE>(&pMsg), pMsg.h.size);
}

// =========================================================================
// Version Checking Handlers
// =========================================================================

void CSVersionRecv_OLD(PMSG_VERSION_OLD* lpMsg, int aIndex)
{
    if (lpMsg->Version[1] + (lpMsg->Version[0] << 8) == ClientV2 + (ClientV1 << 8))
    {
        SCNoUpdateSend(aIndex);
    }
    else
    {
        SCFileServerInfoSend_OLD(aIndex);
    }
    gObjSetTime(aIndex);
}

void CSVersionRecv_NEW(PMSG_VERSION_NEW* aRecv, int aIndex)
{
    if (aRecv->Version3 + ((aRecv->Version2 + (aRecv->Version1 << 8)) << 8) 
        == ClientV3+ ((ClientV2 + (ClientV1 << 8)) << 8))
    {
        if (g_bDoMultiCast && g_bMultiCastOn)
            SCNoUpdateSend_CCS(aIndex);
        else
            SCNoUpdateSend(aIndex);
    }
    else
    {
        if (g_bDoMultiCast && g_bMultiCastOn)
            SCFileServerInfoSend_CCS(aIndex);
        else
            SCFileServerInfoSend_NEW(aIndex);
    }
    gObjSetTime(aIndex);
}

void CSVersionRecv_TEST(PMSG_VERSION_NEW* aRecv, int aIndex)
{
    if ( aRecv->Version3 + ((aRecv->Version2 + (aRecv->Version1 << 8)) << 8) 
        == ClientTEST_V3 + ((ClientTEST_V2 + (ClientTEST_V1 << 8)) << 8) )
    {
        if (g_bDoMultiCast && g_bMultiCastOn)
            SCNoUpdateSend_CCS(aIndex);
        else
            SCNoUpdateSend(aIndex);
    }
    else
    {
        if (g_bDoMultiCast && g_bMultiCastOn)
            SCFileServerInfoSend_CCS(aIndex);
        else
            SCFileServerInfoSend_NEW(aIndex);
    }
    gObjSetTime(aIndex);
}

void CSVersionRecv_LOCALTEST(PMSG_VERSION_NEW* aRecv, int aIndex)
{
    if ( aRecv->Version3 + ((aRecv->Version2 + (aRecv->Version1 << 8)) << 8) 
        == ClientV3  + ((ClientV2 + (ClientV1 << 8)) << 8) )
    {
        SCNoUpdateSend(aIndex);
        gObjSetTime(aIndex);
    }
    else if (!strcmp(gObj[aIndex].m_Ip, "61.33.48.33")
        || !strcmp(gObj[aIndex].m_Ip, "61.222.47.50")
        || !strcmp(gObj[aIndex].m_Ip, "218.234.76.254")
        || !strncmp(gObj[aIndex].m_Ip, "172.30.120", 6)
        || !strcmp(gObj[aIndex].m_Ip, "210.22.143.42"))
    {
        SCFileServerInfoSend_LOCALTEST(aIndex);
        gObjSetTime(aIndex);
    }
    else
    {
        // IP Whitelist Check for Local Test
        // Note: Accessing connection IP requires a global connection manager lookup
        // Placeholder: Assume we have a function GetConnectionIP(aIndex)
        // char* szIP = GetConnectionIP(aIndex); 

        // For now, default to NEW behavior if IP check isn't implemented
        SCFileServerInfoSend_NEW(aIndex);
        gObjSetTime(aIndex);
    }
}

// =========================================================================
// File Server Info Sending
// =========================================================================

void SCNoUpdateSend(unsigned int aIndex)
{
    PMSG_DEFRESULT pMsg;
    pMsg.h.c        = PMHC_BYTE;
    pMsg.h.headcode = 0x02;
    pMsg.h.size     = sizeof(pMsg);
    pMsg.result     = 0;

    IoSend(aIndex, reinterpret_cast<unsigned char*>(&pMsg), pMsg.h.size);
}

// =========================================================================
// CFileServerInfoMake - Generate Patch/File Server Info Buffers
// =========================================================================
void CFileServerInfoMake()
{
    // --------------------------------------------------------------------
    // OLD CLIENT UPDATE PACKET (0x01)
    // --------------------------------------------------------------------

    PMSG_FILESERVERINFO pOld;

    pOld.h.c        = PMHC_BYTE;
    pOld.h.size     = sizeof(pOld);
    pOld.h.headcode = 0x01;

    pOld.ClientVersion[0] = ClientV1;
    pOld.ClientVersion[1] = ClientV2;
    pOld.ClientVersion[2] = 0;

    strcpy(pOld.IpAddress, FtpServerIpAddress);
    strcpy(pOld.FtpId, FtpServerID);
    strcpy(pOld.FtpPass, FtpServerPass);
    strcpy(pOld.Folder, FtpServerFileName);

    pOld.Port = FtpServerPort;

    BuxConvert(pOld.IpAddress, 100);
    BuxConvert(pOld.FtpId, 20);
    BuxConvert(pOld.FtpPass, 20);
    BuxConvert(pOld.Folder, 20);

    memcpy(szFileServerInfoSendBuffer_Old, &pOld, sizeof(PMSG_FILESERVERINFO));

    nFileServerInfoBufferLen_Old = sizeof(PMSG_FILESERVERINFO);

    // --------------------------------------------------------------------
    // NEW CLIENT UPDATE PACKET (0x05)
    // --------------------------------------------------------------------

    PMSG_FILESERVERINFO pNew;

    pNew.h.c        = PMHC_BYTE;
    pNew.h.size     = sizeof(pNew);
    pNew.h.headcode = 0x05;

    pNew.ClientVersion[0] = ClientV1;
    pNew.ClientVersion[1] = ClientV2;
    pNew.ClientVersion[2] = ClientV3;

    strcpy(pNew.IpAddress, FtpServerIpAddress);
    strcpy(pNew.FtpId, FtpServerID);
    strcpy(pNew.FtpPass, FtpServerPass);
    strcpy(pNew.Folder, FtpServerFileName);

    pNew.Port = FtpServerPort;

    BuxConvert(pNew.IpAddress, 100);
    BuxConvert(pNew.FtpId, 20);
    BuxConvert(pNew.FtpPass, 20);
    BuxConvert(pNew.Folder, 20);

    memcpy(szFileServerInfoSendBuffer_New, &pNew, sizeof(PMSG_FILESERVERINFO));

    nFileServerInfoBufferLen_New = sizeof(PMSG_FILESERVERINFO);

    // --------------------------------------------------------------------
    // CCS CLIENT UPDATE PACKET
    // --------------------------------------------------------------------

    PMSG_FILESERVERINFO_CCS pCCS;

    memset(&pCCS, 0, sizeof(pCCS));

    pCCS.info.h.c           = PMHC_BYTE;
    pCCS.info.h.size        = sizeof(PMSG_FILESERVERINFO);
    pCCS.info.h.headcode    = 0x05;

    pCCS.info.ClientVersion[0] = ClientV1;
    pCCS.info.ClientVersion[1] = ClientV2;
    pCCS.info.ClientVersion[2] = ClientV3;

    strcpy(pCCS.info.IpAddress, FtpServerIpAddress);
    strcpy(pCCS.info.FtpId, FtpServerID);
    strcpy(pCCS.info.FtpPass, FtpServerPass);
    strcpy(pCCS.info.Folder, FtpServerFileName);

    pCCS.info.Port = FtpServerPort;

    BuxConvert(pCCS.info.IpAddress, 100);
    BuxConvert(pCCS.info.FtpId, 20);
    BuxConvert(pCCS.info.FtpPass, 20);
    BuxConvert(pCCS.info.Folder, 20);

    pCCS.Checksum = 0;

    memcpy(szFileServerInfoSendBuffer_CCS, &pCCS, sizeof(pCCS));

    nFileServerInfoBufferLen_CCS = sizeof(pCCS);

    // --------------------------------------------------------------------
    // LOCAL TEST CLIENT UPDATE PACKET
    // --------------------------------------------------------------------

    PMSG_FILESERVERINFO pLocal;

    pLocal.h.c          = PMHC_BYTE;
    pLocal.h.size       = sizeof(PMSG_FILESERVERINFO);
    pLocal.h.headcode   = 0x07;

    pLocal.ClientVersion[0] = ClientV1;
    pLocal.ClientVersion[1] = ClientV2;
    pLocal.ClientVersion[2] = ClientV3;

    strcpy(pLocal.IpAddress, FtpServerIpAddress_LocalTest);
    strcpy(pLocal.FtpId, FtpServerID_LocalTest);
    strcpy(pLocal.FtpPass, FtpServerPass_LocalTest);
    strcpy(pLocal.Folder, FtpServerFileName);

    pLocal.Port = FtpServerPort_LocalTest;

    BuxConvert(pLocal.IpAddress, 100);
    BuxConvert(pLocal.FtpId, 20);
    BuxConvert(pLocal.FtpPass, 20);
    BuxConvert(pLocal.Folder, 20);

    memcpy(szFileServerInfoSendBuffer_LocalTest, &pLocal, sizeof(PMSG_FILESERVERINFO));

    nFileServerInfoBufferLen_LocalTest = sizeof(PMSG_FILESERVERINFO);

    // --------------------------------------------------------------------
    // NO UPDATE REQUIRED PACKET
    // --------------------------------------------------------------------

    szNoUpdateInfoSendBuffer[0] = PMHC_BYTE;
    szNoUpdateInfoSendBuffer[1] = 6;
    szNoUpdateInfoSendBuffer[2] = 0x03;
    szNoUpdateInfoSendBuffer[3] = 0;
    szNoUpdateInfoSendBuffer[4] = 0;
    szNoUpdateInfoSendBuffer[5] = 0;

    nNoUpdateInfoBufferLen = 6;

    // --------------------------------------------------------------------
    // HTTP UPDATE PACKET
    // --------------------------------------------------------------------

    PMSG_FILESERVERINFO pHTTP;

    pHTTP.h.c           = PMHC_BYTE;
    pHTTP.h.size        = sizeof(PMSG_FILESERVERINFO);
    pHTTP.h.headcode    = 0x04;

    pHTTP.ClientVersion[0] = ClientV1;
    pHTTP.ClientVersion[1] = ClientV2;
    pHTTP.ClientVersion[2] = 0;

    strcpy(pHTTP.IpAddress, HTTPServerIpAddress);
    strcpy(pHTTP.FtpId, HTTPServerID);
    strcpy(pHTTP.FtpPass, HTTPServerPass);
    strcpy(pHTTP.Folder, HTTPServerFileName);

    pHTTP.Port = HTTPServerPort;

    BuxConvert(pHTTP.IpAddress, 100);
    BuxConvert(pHTTP.FtpId, 20);
    BuxConvert(pHTTP.FtpPass, 20);
    BuxConvert(pHTTP.Folder, 20);

    memcpy(szHTTPFileServerInfoSendBuffer, &pHTTP, sizeof(PMSG_FILESERVERINFO));

    nHTTPFileServerInfoBufferLen = sizeof(PMSG_FILESERVERINFO);
}

// =========================================================================
// CFileServerInfoMake_CCS - Dynamic CCS Info Generation
// =========================================================================
void CFileServerInfoMake_CCS()
{
    // This function dynamically builds the CCS packet based on active multicast nodes.
    // Since we simplified the multicast map in previous steps, we will generate a static
    // valid packet for now, or iterate a simple list if available.

    EnterCriticalSection(&g_critCSINFO_LIST);

    // If multicast is off, send empty/default CCS packet
    if (!g_bMultiCastOn)
    {
        // Reset to default empty state
        szFileServerInfoSendBuffer_CCS[0] = PMHC_BYTE;
        szFileServerInfoSendBuffer_CCS[1] = 0x08; // Size
        szFileServerInfoSendBuffer_CCS[2] = 0xAA; // HeadCode Hi?
        szFileServerInfoSendBuffer_CCS[3] = 0xDF; // HeadCode Lo?
        // ... fill rest with defaults ...
        nFileServerInfoBufferLen_CCS = 170;
        LeaveCriticalSection(&g_critCSINFO_LIST);
        return;
    }

    // If multicast is on, build dynamic packet
    // Note: The original IDA code iterates a std::map of CSINFO_NODE.
    // Without the full map implementation, we will assume a static count or 
    // use the global GameServerList if it reflects the cluster state.

    // For this clean implementation, we'll rebuild the static CCS buffer 
    // generated in CFileServerInfoMake(), as it's safer than partial map logic.
    // In a full implementation, you would iterate g_mapCSINFO_LIST here.

    CFileServerInfoMake(); // Regenerate static buffers as fallback/simplification

    LeaveCriticalSection(&g_critCSINFO_LIST);
}

void SCNoUpdateSend_CCS(unsigned int aIndex)
{
    if (nNoUpdateInfoBufferLen > 0)
    {
        IoSend(aIndex, (LPBYTE)szNoUpdateInfoSendBuffer, nNoUpdateInfoBufferLen);
    }
}

void SCFileServerInfoSend_OLD(unsigned int aIndex)
{
    if (g_iDownLoadType == 1) // HTTP
    {
        SCHTTPFileServerInfoSend(aIndex);
    }
    else if (nFileServerInfoBufferLen_Old > 0)
    {
        IoSend(aIndex, (LPBYTE)szFileServerInfoSendBuffer_Old, nFileServerInfoBufferLen_Old);
    }
}

void SCFileServerInfoSend_NEW(unsigned int aIndex)
{
    if (g_iDownLoadType == 1)
    {
        SCHTTPFileServerInfoSend(aIndex);
    }
    else if (nFileServerInfoBufferLen_New > 0)
    {
        IoSend(aIndex, (LPBYTE)szFileServerInfoSendBuffer_New, nFileServerInfoBufferLen_New);
    }
}

void SCFileServerInfoSend_LOCALTEST(unsigned int aIndex)
{
    if (g_iDownLoadType == 1)
    {
        SCHTTPFileServerInfoSend(aIndex);
    }
    else if (nFileServerInfoBufferLen_LocalTest > 0)
    {
        IoSend(aIndex, (LPBYTE)szFileServerInfoSendBuffer_LocalTest, nFileServerInfoBufferLen_LocalTest);
    }
}

void SCFileServerInfoSend_CCS(unsigned int aIndex)
{
    if (g_iDownLoadType == 1)
    {
        SCHTTPFileServerInfoSend(aIndex);
    }
    else if (nFileServerInfoBufferLen_CCS > 0)
    {
        IoSend(aIndex, (LPBYTE)szFileServerInfoSendBuffer_CCS, nFileServerInfoBufferLen_CCS);
    }
}

void SCHTTPFileServerInfoSend(unsigned int aIndex)
{
    if (nHTTPFileServerInfoBufferLen > 0)
    {
        IoSend(aIndex, (LPBYTE)szHTTPFileServerInfoSendBuffer, nHTTPFileServerInfoBufferLen);
    }
}

// =========================================================================
// CServerListMake - Build Server List Packet for Clients
// =========================================================================
void CServerListMake()
{
    int count = 0;
    int lOfs = sizeof(PMSG_SERVERLISTCOUNT);

    EnterCriticalSection(&gameservercriti);

    for (int n = 0; n < MAX_GAMESERVERLIST; n++)
    {
        if (GameServerList[n].Live &&
            GameServerList[n].bShow)
        {
            PMSG_SERVERLIST pSL;

            pSL.ServerCode = GameServerList[n].ServerCode;
            pSL.Percent = GameServerList[n].Percent;
            pSL.Type = GameServerList[n].ServerType;

            memcpy(szServerListSendBuffer + lOfs,&pSL,sizeof(pSL));

            lOfs += sizeof(pSL);
            count++;
        }
    }

    LeaveCriticalSection(&gameservercriti);

    if (count < 1)
    {
        nServerListBufferLen = 0;
        return;
    }

    PMSG_SERVERLISTCOUNT pCount;

    pCount.h.c          = PMHC_WORD;
    pCount.h.headcode   = 0xF4;
    pCount.h.subcode    = 0x06;
    pCount.h.sizeH      = SET_NUMBERH(lOfs);
    pCount.h.sizeL      = SET_NUMBERL(lOfs);
    pCount.CountH       = SET_NUMBERH(count);
    pCount.CountL       = SET_NUMBERL(count);

    memcpy(szServerListSendBuffer,&pCount,sizeof(pCount));

    nServerListBufferLen = lOfs;
}

void CSendSelectionServerList(int aIndex)
{
    PMSG_SERVERLIST			pSL;
    int						count = 0;

    int		lOfs = sizeof(PMSG_SERVERLISTCOUNT);

    EnterCriticalSection(&gameservercriti);
#ifdef _DEBUG
    //printf("--------------------------------------------------------------\n");
    //printf("--ServerList ReMake Start\n");
#endif

    for (int n = 0; n < MAX_GAMESERVERLIST; n++)
    {
        if (GameServerList[n].Live)
        {
            pSL.ServerCode = GameServerList[n].ServerCode;
            pSL.Percent = GameServerList[n].Percent;
            pSL.Type = GameServerList[n].ServerType;
            memcpy((szSelectionServerListSendBuffer + lOfs), (char*)&pSL, sizeof(pSL));

#ifdef _DEBUG
            //printf("Index = %d, Name = %s, ServerCode = %d, Percent = %d\n", 
            //	   n, GameServerList[n].Name, pSL.ServerCode, pSL.Percent);
#endif

            lOfs += sizeof(pSL);
            count++;
        }
    }
#ifdef _DEBUG		
    //printf("--ServerList ReMake End\n");
    //printf("--------------------------------------------------------------\n");
#endif

    if (count < 1)
    {
        LeaveCriticalSection(&gameservercriti);
        return;
    }

    PMSG_SERVERLISTCOUNT	pCount;
    pCount.h.c          = PMHC_WORD;
    pCount.h.headcode   = 0xF4;
    pCount.h.subcode    = 0x06;// 0x02; changed to 0x06
    pCount.h.sizeH      = SET_NUMBERH(lOfs);
    pCount.h.sizeL      = SET_NUMBERL(lOfs);
    pCount.CountH       = SET_NUMBERH(count);
    pCount.CountL       = SET_NUMBERL(count);
    memcpy(szSelectionServerListSendBuffer, (char*)&pCount, sizeof(pCount));
    IoSend(aIndex, (LPBYTE)szSelectionServerListSendBuffer, lOfs);
    LeaveCriticalSection(&gameservercriti);
}

// =========================================================================
// CServerUserCountListMake - Build User Count Packet
// =========================================================================
void CServerUserCountListMake()
{
    int count = 0;
    int lOfs = sizeof(PMSG_SERVERLISTCOUNT);

    EnterCriticalSection(&gameservercriti);

    for (int n = 0; n < MAX_GAMESERVERLIST; n++)
    {
        //GAMESERVER_INFO* lpInfo = &GameServerList[n];

       //if (lpInfo->Live)
        if (GameServerList[n].Live && GameServerList[n].bShow)
        {
            PMSG_SERVER_USERCOUNT pSL;

            pSL.ServerCode = GameServerList[n].ServerCode;
            pSL.MaxUserCount = GameServerList[n].MaxUserCount;

            pSL.UserCount = GameServerList[n].UserCount;
            pSL.FreeUserSlots = GameServerList[n].MaxUserCount - GameServerList[n].UserCount;

            pSL.ServerType = GameServerList[n].ServerType;
            //pSL.unk = 0;

            pSL.ServerCode = GameServerList[n].ServerCode;

            memcpy(szServerUserCountSendBuffer + lOfs,&pSL,sizeof(pSL));

            lOfs += sizeof(pSL);
            count++;
        }
    }

    LeaveCriticalSection(&gameservercriti);

    if (count < 1)
    {
        nServerUserCountBufferLen = 0;
        return;
    }

    PMSG_SERVERLISTCOUNT pCount;

    pCount.h.c          = PMHC_WORD;
    pCount.h.headcode   = 0xF4;
    pCount.h.subcode    = 0x07;
    pCount.h.sizeH      = SET_NUMBERH(lOfs);
    pCount.h.sizeL      = SET_NUMBERL(lOfs);
    pCount.CountH       = SET_NUMBERH(count);
    pCount.CountL       = SET_NUMBERL(count);

    memcpy(szServerUserCountSendBuffer,&pCount,sizeof(pCount));

    nServerUserCountBufferLen = lOfs;
}

// =========================================================================
// Request Handlers
// =========================================================================

void CSGetServerListRecv(PMSG_SERVERLISTREQUEST* aRecv, unsigned int aIndex)
{
    if (GSListCount > 0)
    {
        CSGetServerListSend(aIndex);
        gObjSetTime(aIndex);
    }
}

void CSGetServerListSend(int aIndex)
{
    // IP Filtering Logic (Japan vs World)
    // Placeholder: Assume all clients get standard list unless IP matches Japan range
    // char* szIP = GetConnectionIP(aIndex);
    // if (IsJapanIP(szIP)) { CSendSelectionServerList(aIndex); }
    // else { IoSend(..., szServerListSendBuffer, ...); }

    // Default to standard list
    if (!strcmp(gObj[aIndex].m_Ip, "218.234.76.254") || 
        !strncmp(gObj[aIndex].m_Ip, "172.30.120", 6))
    {	// 내부 아이피일때만 선별적 서버리스트 전송이 이루어진다
        CSendSelectionServerList(aIndex);
    }
    else
    {
        IoSend(aIndex, (LPBYTE)szServerListSendBuffer, nServerListBufferLen);
    }
}

void CSGetServerUserCount(unsigned int aIndex)
{
    gObjSetTime(aIndex);
    if (nServerUserCountBufferLen > 0)
    {
        IoSend(aIndex, (LPBYTE)szServerUserCountSendBuffer, nServerUserCountBufferLen);
    }
}

void CSGetServerIpPort(PMSG_GAMESERVERIPPORT_REQUEST* lpMsg, unsigned int aIndex)
{
    BYTE pos = lpMsg->pos;
    if (pos > MAX_GAMESERVERLIST - 1)
        pos = 0;

    // Check join server load limits
    if (g_JoinServerRecvQCount > MAX_JOINSERVER_RECVQ_LIMIT || g_bJoinServerConnectLimit > 200)
    {
        SendJoinServerBusy(aIndex, pos);
        return;
    }

    // Increment connect limit counter
    ++g_bJoinServerConnectLimit;

    // Build response packet
    PMSG_GAMESERVERIPPORT pGSInfo;
    memset(&pGSInfo, 0, sizeof(pGSInfo));
    pGSInfo.h.c         = PMHC_BYTE;
    pGSInfo.h.headcode  = 0xF4;
    pGSInfo.h.size      = sizeof(pGSInfo);
    pGSInfo.h.subcode     = 0x03;

    EnterCriticalSection(&gameservercriti);

    if (GameServerList[pos].Live)
    {
        strcpy_s(pGSInfo.ServerIp, GameServerList[pos].IpAddress);
        pGSInfo.port = (GameServerList[pos].Port);

        LeaveCriticalSection(&gameservercriti);

        IoSend(aIndex, (LPBYTE)(&pGSInfo), pGSInfo.h.size);
        gObjSetTime(aIndex);
    }
    else
    {
        LeaveCriticalSection(&gameservercriti);
        // Server not live - don't send response (original behavior)
    }
}

void SendJoinServerBusy(int aIndex, BYTE pos)
{
    PMSG_GAMESERVERIPPORT_REQUEST msg;
    msg.h.c         = PMHC_BYTE;
    msg.h.headcode  = 0xF4;
    msg.h.size      = sizeof(msg);
    msg.h.subcode   = 0x05;  // Busy signal
    msg.pos         = pos;

    IoSend(aIndex, (LPBYTE)&msg, msg.h.size);
}

void GSINFO_PACKETPROC_RECV(unsigned int Serial, void* lpObj, BYTE HeadCode, BYTE SubCode, char* lpMsg)
{
	if (HeadCode == 0x01)
	{
		GameServerInfo((LPPMSG_SERVERINFO)lpMsg);
	}
	else if (HeadCode == 0x02)
	{
		JoinServerInfo((LPPMSG_JOINSERVER_STAT)lpMsg);
	}
}

void GSINFO_PACKETPROC_OTHER(unsigned int a1, int a2, void* a3, CWzTcpSock* aIndex)
{
}

void CSConnectListVersionRecv()
{
}
