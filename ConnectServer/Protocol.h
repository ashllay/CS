#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__
#include "WzTcpSock.h"
#include "..\\include\\ProDef.h"

typedef struct
{
    PBMSG_HEAD	h;
    BYTE		result;
} PMSG_DEFRESULT, * LPPMSG_DEFRESULT;

//----------------------------------------------------------------------------
// [0x00] 접속 서버에 접속을 시도했다면 접속 결과를 알린다.
//----------------------------------------------------------------------------
typedef PMSG_DEFRESULT	PMSG_CONNECTRESULT;

//----------------------------------------------------------------------------
// [0x01] 클라이언트가 버젼 정보를 보낸다.
//----------------------------------------------------------------------------
struct PMSG_VERSION_OLD {
    BYTE c;
    BYTE size;
    BYTE headcode;
    BYTE subcode;
    WORD Version; // ClientV1 << 8 | ClientV2
};

struct PMSG_VERSION_NEW {
    BYTE c;
    BYTE size;
    BYTE headcode;
    BYTE subcode;
    WORD VersionHi; // ClientV3 << 8 | ClientV2
    BYTE VersionLo; // ClientV1
};

struct GAMESERVER_INFO
{
    BOOL Live;              // -4
    WORD ServerCode;        // +0
    BYTE unk02[40];
    BYTE ServerType;        // +42
    BYTE unk2B[5];
    WORD MaxUserCount;      // +48
    BYTE unk32[2];
    WORD UserCount;         // +52
    BYTE unk36[18];
    WORD ServerFlag;        // +72
    BYTE unk4A[134];
};

// [0x02] Server list count header (word-size)
typedef struct
{
    PWMSG_HEAD h;
    BYTE subcode;
    BYTE Count;
} PMSG_SERVERLISTCOUNT;

struct PMSG_SERVERLIST
{
    WORD ServerCode;
    BYTE Percent;
    BYTE Type; // or NonPVP/PK/ServerKind/etc
};


#pragma pack(push, 1)

typedef struct
{
    DWORD ServerCode;

    WORD MaxUserCount;
    WORD FreeUserSlots;

    WORD UserCount;

    BYTE ServerType;
    BYTE unk;

    WORD ServerFlag;

} PMSG_SERVER_USERCOUNT;

#pragma pack(pop)

#pragma pack(push, 1)

struct PMSG_FILESERVERINFO
{
    PBMSG_HEAD h;

    BYTE ClientVersion[3];

    char IpAddress[100];

    WORD Port;

    char FtpId[20];
    char FtpPass[20];
    char Folder[20];
};

#pragma pack(pop)


struct PMSG_FILESERVERINFO_CCS
{
    PMSG_FILESERVERINFO info;
    WORD Checksum;
};


// [0xF4][0x03] Request game server IP/Port
struct PMSG_GAMESERVERIPPORT_REQUEST {
    PBMSG_HEAD h;
    BYTE subcode;
    BYTE pos;  // Server index
};

// [0xF4][0x03] Response with game server IP/Port
struct PMSG_GAMESERVERIPPORT {
    PBMSG_HEAD h;
    BYTE subcode;
    char ServerIp[15];
    WORD port;
};
// [0x02] Server list request
struct PMSG_SERVERLISTREQUEST {
    PBMSG_HEAD h;
};

#pragma pack(push, 1)

typedef struct
{
    PBMSG_HEAD h;          // +0

    WORD ServerCode;       // +4
    BYTE Percent;          // +6
    BYTE Padding07;        // +7

    WORD MaxUserCount;     // +8
    WORD FreeUserSlots;    // +10
    WORD Reserved;         // +12

    WORD UserCount;        // +14

} PMSG_SERVERINFO, * LPPMSG_SERVERINFO;

#pragma pack(pop)

//----------------------------------------------------------------------------
// CS UDP [0x02] 인증 서버의 RecvQ 카운트를 전송 받는다
//----------------------------------------------------------------------------
typedef struct
{
    PBMSG_HEAD	h;
    int			iQueueCount;
} PMSG_JOINSERVER_STAT, * LPPMSG_JOINSERVER_STAT;


#pragma pack(push, 1)

struct PMSG_CONNECTSERVER_INFO
{
    PBMSG_HEAD h;

    DWORD MultiCastIP;
    DWORD PersonalShopIP;

    int ConnectionCount;
    int CpuUsage;
    int MemoryUsage;
};

#pragma pack(pop)

// Function Prototypes
void ProtocolCore(unsigned char protoNum, unsigned char* aRecv, int aLen, short aIndex);
void UpdateGameServerList(int aIndex);
void ReqGameServerList(int aIndex);
void SCConnectResultSend(char result, unsigned int aIndex);
void CSVersionRecv_OLD(PMSG_VERSION_OLD* lpMsg, int aIndex);
void CSVersionRecv_NEW(PMSG_VERSION_NEW* aRecv, int aIndex);
void CSVersionRecv_TEST(PMSG_VERSION_NEW* aRecv, int aIndex);
void CSVersionRecv_LOCALTEST(PMSG_VERSION_NEW* aRecv, int aIndex);
void SCNoUpdateSend(unsigned int aIndex);
void CFileServerInfoMake();
void CFileServerInfoMake_CCS();
void CServerListMake();
void CServerUserCountListMake();
void SCFileServerInfoSend_OLD(unsigned int aIndex);
void SCFileServerInfoSend_NEW(unsigned int aIndex);
void SCFileServerInfoSend_LOCALTEST(unsigned int aIndex);
void SCFileServerInfoSend_CCS(unsigned int aIndex);
void SCNoUpdateSend_CCS(unsigned int aIndex);
void SCHTTPFileServerInfoSend(unsigned int aIndex);
void CSGetServerListRecv(PMSG_SERVERLISTREQUEST* aRecv, unsigned int aIndex);
void CSGetServerListSend(unsigned int aIndex);
void CSGetServerUserCount(unsigned int aIndex);
void CSGetServerIpPort(PMSG_GAMESERVERIPPORT_REQUEST* lpMsg, unsigned int aIndex);
void SendJoinServerBusy(int aIndex, BYTE pos);
void GSINFO_PACKETPROC_RECV(unsigned int Serial, void* lpObj, BYTE HeadCode, BYTE SubCode, char* lpMsg);
void GSINFO_PACKETPROC_OTHER(unsigned int a1, int a2, void* a3, CWzTcpSock* aIndex); // idb

void CSConnectListVersionRecv();

#define MAX_JOINSERVER_RECVQ_LIMIT		150

#endif // __PROTOCOL_H__