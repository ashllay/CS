#include "StdAfx.h"
#include "udpprotocol.h"

#include "CS.h"
#include "LogProc.h"
#include "LogToFile.h"

void UdpProtocolCore(BYTE protoNum, BYTE* aRecv, int aLen, char* ip)
{
    switch (protoNum)
    {
    case 1:
        GameServerInfo((PMSG_SERVERINFO*)aRecv);
        break;
    case 2:
        JoinServerInfo((PMSG_JOINSERVER_STAT*)aRecv);
        break;
    case 3:
        CSGetServerExtInfo((PMSG_GAMESERVER_EXTINFO*)aRecv);
        break;
    }
}


void GameServerInfo(PMSG_SERVERINFO* lpMsg)
{
	GameServerInfoUpdate(
		lpMsg->ServerCode,
		lpMsg->Percent,
		lpMsg->UserCount,
		0,
		0,
		lpMsg->MaxUserCount);

	CServerListMake();
}

void JoinServerInfo(PMSG_JOINSERVER_STAT* lpMsg)
{
	g_JoinServerRecvQCount = lpMsg->iQueueCount;

	if (!g_bJoinServerLive)
	{
		g_LogProc.LogAddFunc("JoinServer udpate complete.");
		g_bJoinServerLive = TRUE;
	}

	g_bJoinServerConnectLimit = 0;
	g_dwJoinServerRefreshTime = GetTickCount();
}

void CSGetServerExtInfo(PMSG_GAMESERVER_EXTINFO* lpMsg)
{
	EnterCriticalSection(&gameservercriti);

	for (int n = 0; n < MAX_GAMESERVERLIST; n++)
	{
		if (GameServerList[n].ServerCode == lpMsg->ServerCode)
		{
			GameServerList[n].ExtInfoReceived = 1;
			GameServerList[n].IsPK = lpMsg->btIsPK;

			// NOTE: The original checks ClientVersion here but copies ServerVersion below.
			if (strcmp(lpMsg->ClientVersion, "") == 0)
			{
				memset(GameServerList[n].ServerVersion,0,sizeof(GameServerList[n].ServerVersion));
			}
			else
			{
				strcpy(GameServerList[n].ServerVersion,lpMsg->ServerVersion);
			}

			if (strcmp(lpMsg->ClientVersion, "") == 0)
			{
				memset(GameServerList[n].ClientVersion,0,sizeof(GameServerList[n].ClientVersion));
			}
			else
			{
				strcpy(GameServerList[n].ClientVersion,lpMsg->ClientVersion);
			}
			break;
		}
	}
	LeaveCriticalSection(&gameservercriti);
}

void WriteGameServerInfo()
{
	EnterCriticalSection(&gameservercriti);

	g_LogGSInfo.Output("==========Game Server Info Write Start==========");

	for (int n = 0; n < MAX_GAMESERVERLIST; n++)
	{
		if (GameServerList[n].Live && GameServerList[n].bShow)
		{
			int MainServer = GameServerList[n].ServerCode / 20 + 1;
			int SubServer = GameServerList[n].ServerCode % 20 + 1;

			g_LogGSInfo.Output(
				"Server:%d SubServer:%d, PersonUserCount:%d, PCRoomUserCount:%d",
				MainServer,
				SubServer,
				GameServerList[n].UserCount,        // PersonUserCount
				GameServerList[n].PCbangCount);   // PCRoomUserCount
		}
	}

	g_LogGSInfo.Output("----------Game Server Info Write End----------");

	LeaveCriticalSection(&gameservercriti);
}