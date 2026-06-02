#pragma once

#ifndef __CS_H__
#define __CS_H__
#include "StdAfx.h"
#include "WzMultiCastSock.h"

#define CONNECTSERVER_PORT			44405		// Connect Server Port
#define CONNECTSERVER_UDP_PORT		55557		// Connect Server UDPPort
// Game server list structure (reconstructed from IDA offsets)
#define MAX_GAMESERVERLIST 1000
#define GAME_SERVER_ENTRY_SIZE 208  // Matches original stride

// =========================================================================
// Global Variables
// =========================================================================
extern int g_iPERF_IDX_MEM;
extern BYTE ClientV3, ClientV1, ClientV2;
extern char FtpServerIpAddress_LocalTest[];
extern char FtpServerIpAddress[];
extern WORD HTTPServerPort;
extern CRITICAL_SECTION gameservercriti;
extern int g_iPERF_IDX_CPU;
extern CRITICAL_SECTION g_critCSINFO_LIST;
extern char HTTPServerFileName[];
extern char FtpServerFileName[];
extern char HTTPServerPass[];
extern char FtpServerID_LocalTest[];
extern BYTE ClientTEST_V1, ClientTEST_V3, ClientTEST_V2;
extern char ClientVersion[];
extern char ClientVersion_TEST[];
extern char HTTPServerID[];
extern char HTTPServerIpAddress[];
extern BOOL g_lpMultiCastRecvNotifyProc;
extern BOOL g_lpMultiCastErrorNotifyProc;
extern int g_iDownLoadType;
extern char FtpServerPass[];
extern WORD FtpServerPort_LocalTest;
extern BOOL g_bDoTerminateLoopThread;
extern int g_nICON_IMAGEID;
extern DWORD g_dwMaxPacketPerSec;   // Max packets/sec
extern DWORD g_dwMaxInvalidPackets;  // Max invalid packets
extern DWORD g_dwBanDurationMs;
extern char DDOSNoCheckingIpAddress[];
extern char FtpServerID[];
extern WORD FtpServerPort;
extern char FtpServerPass_LocalTest[];
extern int GSListCount;
extern int g_JoinServerRecvQCount;
extern DWORD g_dwJoinServerRefreshTime;
extern int g_bJoinServerLive;
extern int g_bJoinServerConnectLimit;
extern int g_bLocalNetworkConnectOnly;
extern int g_bLocalConnectConfirm;
extern int g_bDoMultiCast;
extern int g_bMultiCastOn;
extern char g_szMC_IPADDR[];
extern char g_szPB_IPADDR[];
extern int g_iLV_ItemNo;
extern BOOL g_bMULTICAST_LIST_REMAKE;
extern BOOL g_bGameServerLive;          // Game server heartbeat active


#pragma pack(push, 1)
struct PMSG_CCS_INFOLIST
{
    int   ServerCode;       // +0
    BYTE  ServerType;       // +4
    BYTE  unk5;
    BYTE  unk6;
    BYTE  unk7;

    WORD  MaxUserCount;     // +8
    WORD  PCbangCount;    // +10
    WORD  UserCount;        // +12
    WORD  ServerFlag;       // +14
};

struct PMSG_CCS_INFOSHARE
{
    BYTE c;                 // C2
    BYTE sizeH;
    BYTE sizeL;
    BYTE head;              // 03

    int  JoinServerCount;   // +4
    int  ShareCount;        // +8

    PMSG_CCS_INFOLIST Entry[100];
};
#pragma pack(pop)

extern PMSG_CCS_INFOSHARE g_cCCS_INFOSHARE_BUF;
//#pragma pack(push, 1)

typedef struct
{
    BOOL  Live;
    DWORD ServerCode;
    char  Name[20];
    char  IpAddress[16];
    WORD  Port;
    BYTE  Percent;
    BYTE  ServerType;
    DWORD LiveTime;
    WORD  MaxUserCount;
    WORD  PCbangCount;
    WORD  UserCount;
    BYTE  bShow;
    WORD  ServerFlag;
    BYTE  State;
    char  ServerVersion[32];
    char  ClientVersion[32];
    char  DisplayName[80];

} GameServerList_Struct;

//#pragma pack(pop)

extern GameServerList_Struct	GameServerList[];


// =========================================================================
// Multicast/CS Info Structures
// =========================================================================
#pragma pack(push, 1)

struct CSINFO_NODE
{
    DWORD MultiCastIP;       // +0x00
    DWORD PersonalShopIP;    // +0x04
    int ConnectionCount;     // +0x08
    int CpuUsage;            // +0x0C
    int MemoryUsage;         // +0x10
    DWORD dwTickCount;       // +0x14
};

#pragma pack(pop)

//extern std::map<DWORD, CSINFO_NODE> g_mapCSINFO_LIST;

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd);

// Dialog Procedures
BOOL CALLBACK MainDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ReloadDataDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ErrorLogDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK McIpSelectDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK PbIpSelectDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Initialization & UI
void IpSelectDlg_InsertListViewCtrl(in_addr addrMulticast, in_addr addrPublic, int nConnectCount, int nCPU, int nMem);
void IpSelectDlg_InitListViewCtrl();
void InitDlgControls(HWND hWnd);
void ShowGameServerListToTreeView();
void CheckNShowPublicIPAddress();
void CheckLocalNetworkConnectOnly();
void WriteGameServerInfo();
void ReadServerList();
int ReadServerInfo();
void ReadConnServerList();

// Network & Multicast
void MULTICAST_RECVNOTIFY_PROC(CWzMultiCastSock* lpMultiCast, int IpAddress, int Protocol, int SubCode, char* lpMsg, int Size);
void MULTICAST_ERRORNOTIFY_PROC(CWzMultiCastSock* lpMultiCast, int ErrorCode, char* szError);
void MC_SEND_ConnectServerInfo();
void MC_SEND_CcsShareInfo();
BOOL MakeCcsShareInfoBuffer();
void CFileServerInfoMake_CCS();
void MC_ShowConnectServerList();
void IpSelectDlg_ClearListView();

void GameServerLoop();
void GameServerLoopEnd();
void GameServerInfoUpdate(int servercode, BYTE percent, WORD maxuser, WORD usercount, WORD freeusers, WORD serverflag);

// Error Log Alarm
void StartErrorLogAlarm();
void EndErrorLogAlarm();

// Thread Entry Point
BOOL CALLBACK MainThreadProc(LPARAM* pParam);
DWORD CALLBACK LoopThread(LPVOID lpParam);

#endif // __CS_H__