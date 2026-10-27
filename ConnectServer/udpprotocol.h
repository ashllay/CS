#ifndef __UDPPROTOCOL_H__
#define __UDPPROTOCOL_H__
#include "WzUdp.h"
#include "Protocol.h"
#include "WzUdp.h"

#define SERVERCODE_EXPAND

typedef struct
{
    PBMSG_HEAD	h;
#ifdef SERVERCODE_EXPAND	
    short	ServerCode;
#else
    BYTE	ServerCode;
#endif
    BYTE	Percent;
    short	UserCount;
    short   AccountCount;
    short	PCbangCount;
    short	MaxUserCount;
} PMSG_SERVERINFO, * LPPMSG_SERVERINFO;
//----------------------------------------------------------------------------
// CS UDP [0x02] 인증 서버의 RecvQ 카운트를 전송 받는다
//----------------------------------------------------------------------------

typedef struct
{
	PBMSG_HEAD	h;
	BYTE		subcode;
	DWORD		ServerCode;
	BYTE		btIsPK;
	char		ServerVersion[32];
	char		ClientVersion[32];

} PMSG_GAMESERVER_EXTINFO, * LPPMSG_GAMESERVER_EXTINFO;

void UdpProtocolCore(BYTE protoNum, BYTE* aRecv, int aLen, char* ip);
void GameServerInfo(PMSG_SERVERINFO* lpMsg);
void JoinServerInfo(PMSG_JOINSERVER_STAT* lpMsg);
void CSGetServerExtInfo(PMSG_GAMESERVER_EXTINFO* lpMsg);

void WriteGameServerInfo();

#endif
