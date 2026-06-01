#ifndef __UDPPROTOCOL_H__
#define __UDPPROTOCOL_H__
#include "WzUdp.h"
#include "Protocol.h"
#include "WzUdp.h"

#pragma pack(push, 1)
typedef struct
{
	PBMSG_HEAD h;
	DWORD ServerCode;
	BYTE ServerType;
	char ServerVersion[32];
	char ClientVersion[32];

} PMSG_GAMESERVER_EXTINFO, * LPPMSG_GAMESERVER_EXTINFO;
#pragma pack(pop)

void UdpProtocolCore(BYTE protoNum, BYTE* aRecv, int aLen, char* ip);
void GameServerInfo(PMSG_SERVERINFO* lpMsg);
void JoinServerInfo(PMSG_JOINSERVER_STAT* lpMsg);
void CSGetServerExtInfo(PMSG_GAMESERVER_EXTINFO* lpMsg);

void WriteGameServerInfo();

#endif
