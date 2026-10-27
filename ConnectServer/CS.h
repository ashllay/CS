#pragma once

#ifndef __CS_H__
#define __CS_H__
#include "StdAfx.h"
#include <CommCtrl.h>
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
extern int g_iPERF_IDX_CPU;
extern BYTE ClientV3, ClientV1, ClientV2;
extern char FtpServerIpAddress_LocalTest[];
extern char FtpServerIpAddress[];
extern WORD HTTPServerPort;
extern CRITICAL_SECTION gameservercriti;
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
extern int g_bMULTICAST_LIST_REMAKE;
extern bool g_bGameServerLive;          // Game server heartbeat active


#pragma pack(push, 1)
struct PMSG_CCS_INFOLIST
{
    int   ServerCode;       // +0
    int   ServerType;       // +4 (BYTE Percent zero-extended)
    WORD  MaxUserCount;     // +8
    WORD  FreeUserSlots;    // +10 (MaxUserCount - UserCount)
    WORD  UserCount;        // +12
    WORD  ServerFlag;       // +14
};

struct PMSG_CCS_INFOSHARE
{
    PWMSG_HEAD h;           // +0  C2 sizeH sizeL headcode(3)
    DWORD  Unknown4;        // +4  never written by MakeCcsShareInfoBuffer
    int  JoinServerCount;   // +8
    int  ShareCount;        // +12

    PMSG_CCS_INFOLIST Entry[MAX_GAMESERVERLIST];   // +16
};
#pragma pack(pop)

extern PMSG_CCS_INFOSHARE g_cCCS_INFOSHARE_BUF;

struct GameServerList_Struct
{
    BOOL  Live;                 // 0x00
    int   ServerCode;           // 0x04
    char  Name[20];             // 0x08
    char  IpAddress[16];        // 0x1C
    WORD  Port;                 // 0x2C
    BYTE  Percent;              // 0x2E
    BYTE  Unknown2F;            // 0x2F
    DWORD LiveTime;             // 0x30
    WORD  MaxUserCount;         // 0x34
    WORD  PCbangCount;          // 0x36
    WORD  UserCount;            // 0x38
    BYTE  bShow;                // 0x3A
    char  ServerLocation[17];   // 0x3B
    WORD  ServerFlag;           // 0x4C
    BYTE  ExtInfoReceived;      // 0x4E
    BYTE  IsPK;                 // 0x4F
    char  ServerVersion[32];    // 0x50
    char  ClientVersion[32];    // 0x70
    char  DisplayName[64];      // 0x90
};

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

extern std::map<int, CSINFO_NODE> g_mapCSINFO_LIST;
extern std::map<int, CSINFO_NODE> g_mapCSINFO_LIST_OLD;

// Dialog Procedures
BOOL CALLBACK MainDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ReloadDataDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK ErrorLogDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK McIpSelectDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK PbIpSelectDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AboutDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Initialization & UI
void IpSelectDlg_InsertListViewCtrl(in_addr addrMulticast, in_addr addrPublic, int nConnectCount, int nCPU, int nMem);
void IpSelectDlg_InitListViewCtrl();
void InitDlgControls(HWND hWnd);
void ShowGameServerListToTreeView();
void InverseGameServer_ShowHide(HWND hWnd, HTREEITEM& hItem, char* ServerName);
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
int  MakeCcsShareInfoBuffer();
void MC_ShowConnectServerList();
void IpSelectDlg_ClearListView();

void GameServerLoop();
void GameServerLoopEnd();
void GameServerInfoUpdate(int servercode, int percent, WORD maxuser, WORD usercount, WORD freeusers, WORD serverflag);

// Error Log Alarm
void StartErrorLogAlarm();
void EndErrorLogAlarm();

// Thread Entry Point
unsigned int __stdcall MainThreadProc(void* pParam);
DWORD CALLBACK LoopThread(LPVOID lpParam);

#endif // __CS_H__