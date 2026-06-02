#include "StdAfx.h"
#include "Protocol.h"
#include "giocp.h"
#include "WzUdp.h"
#include <CommCtrl.h>
#include "DefaultWindow.h"
#include "WzTcpSock.h"
#include "IpAddrMng.h"
#include "PerfMonEx.h"
#include "TrayIconControl.h"
#include "TreeViewControl.h"
#include "User.h"
#include "WhatsUpDummyServer.h"
#include "WzMemScript.h"
#include "CS.h"

// =========================================================================
// Global Variables Definition
// =========================================================================
HWND hErrorLogDlg;
HINSTANCE g_hInst;
int g_iPERF_IDX_MEM;
BYTE ClientV3, ClientV1, ClientV2;
char FtpServerIpAddress_LocalTest[104];
WzUdp cUdpSoc;
HANDLE LoopThreadHandle;
char FtpServerIpAddress[102];
WORD HTTPServerPort;
//CRITICAL_SECTION g_critERROR_LOG = { NULL, 0, 0, NULL, NULL, 0 };//not the right place for this
CRITICAL_SECTION gameservercriti;
int g_iPERF_IDX_CPU;
CWzTcpSock g_GameServerInfoSock;
CRITICAL_SECTION g_critCSINFO_LIST;
char HTTPServerFileName[24];
char FtpServerFileName[24];
CPerfMonEx g_PerfMonEx;
char HTTPServerPass[28];
CDefaultWindow CDW;
HWND hReloadDataDlg;
char FtpServerID_LocalTest[21];
BYTE ClientTEST_V1, ClientTEST_V3, ClientTEST_V2;
GameServerList_Struct	GameServerList[MAX_GAMESERVERLIST];				// 일반 서버리스트 전송용
char ClientVersion[12];
char ClientVersion_TEST[16];
char HTTPServerID[24];
char HTTPServerIpAddress[18];
CWzMultiCastSock g_CsInfoMultiCast;
BOOL g_lpMultiCastRecvNotifyProc = TRUE;
BOOL g_lpMultiCastErrorNotifyProc = TRUE;
HWND hToolBar;
int g_iDownLoadType;
CTreeViewControl CTreeCtl;
HWND hWnd;
char FtpServerPass[24];
HWND hMainDlg;
WORD FtpServerPort_LocalTest;
BOOL g_bDoTerminateLoopThread = FALSE;
CWhatsUpDummyServer g_WhatUpDummyServer;
HWND hAboutDlg;

int g_nICON_IMAGEID;
DWORD g_dwMaxPacketPerSec = 30;   // Max packets/sec
DWORD g_dwMaxInvalidPackets = 100;  // Max invalid packets
DWORD g_dwBanDurationMs;
char DDOSNoCheckingIpAddress[104];
char FtpServerID[28];
CTrayIconControl CTrayCtl;
WORD FtpServerPort;
char FtpServerPass_LocalTest[24];
int GSListCount;
int g_JoinServerRecvQCount;
DWORD g_dwJoinServerRefreshTime;
int g_bJoinServerLive;
int g_bJoinServerConnectLimit;
int g_bLocalNetworkConnectOnly;
int g_bLocalConnectConfirm;
int g_bDoMultiCast;
int g_bMultiCastOn;
CHAR g_szMC_IPADDR[32];
CHAR g_szPB_IPADDR[32];
int g_iLV_ItemNo = 0;
BOOL g_bMULTICAST_LIST_REMAKE = FALSE;
BOOL g_bGameServerLive = TRUE;          // Game server heartbeat active
HWND hMcIpSelectDlg;
HWND hPbIpSelectDlg;

PMSG_CCS_INFOSHARE g_cCCS_INFOSHARE_BUF;

//CIpAddrMng g_CsIpAddrMng;//not the right place for this

// Buffer for string formatting
static char String[256];
// =========================================================================
// WinMain - Application Entry Point
// =========================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	HANDLE hMutex = CreateMutexA(NULL, FALSE, "WZ_CS_MUTEX");

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		CloseHandle(hMutex);
		MessageBoxA(NULL, "Webzen Connect Server Server is already Running.", "Error", MB_ICONSTOP | MB_OK);
		return 0;
	}

	g_hInst = hInstance;

	// Initialize common controls (your addition — keep it, it's needed for TreeView/Toolbar)
	INITCOMMONCONTROLSEX icex;
	icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
	icex.dwICC = ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	// Create main dialog (0x65 = 101)
	DialogBoxParamA(hInstance, MAKEINTRESOURCEA(IDD_MAIN_DIALOG), NULL, (DLGPROC)MainDlgProc, 0);

	CloseHandle(hMutex);
	return 0;
}

// =========================================================================
// MainDlgProc - Main Dialog Window Procedure
// =========================================================================
INT_PTR CALLBACK MainDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HWND DlgItem;
	HWND hList;
	HANDLE hThread;
	HWND hEdit;

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		hMainDlg = hWnd;

		CDW.mfn_CreateWindow(g_hInst, hWnd, 250, 125, 700, 500, WS_CHILD | WS_VISIBLE | WS_BORDER);

		g_LogProc.SetLogWindow(CDW.m_hWnd);  // Set the window handle

		DlgItem = GetDlgItem(hWnd, IDC_TREE_SERVER_LIST);

		CTreeCtl.Start(DlgItem, hWnd);

		CTreeCtl.AddImagetoImageList(LoadIcon(g_hInst, MAKEINTRESOURCE(IDB_BITMAP_TOOLBAR)));//107?
		CTreeCtl.AddImagetoImageList(LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON_STATE_ONLINE)));//132?
		CTreeCtl.AddImagetoImageList(LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_ICON_STATE_ERROR)));//105?

		g_OldTreeViewProc = (WNDPROC)SetWindowLong(hWnd, GWL_WNDPROC, (LONG)CTreeViewControl::TreeViewSubProc);

		CTrayCtl.Start(g_hInst, hWnd, IDI_ICON_STATE_ONLINE, "ConnectServer OK");

		InitDlgControls(hWnd);

		if (!IsWindow(hErrorLogDlg))
		{
			hErrorLogDlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_ERROR_LOG_DIALOG), hWnd, ErrorLogDlgProc, 0);
		}

		ShowWindow(hErrorLogDlg, SW_HIDE);

		//dword_449830 = dword_483E30;

		g_LogProc.SetParentWindowBitmap();

		hList = GetDlgItem(hErrorLogDlg, IDC_LIST_ERROR_LOG);

		g_LogListBoxError.SetListBox(hList);

		InitializeCriticalSection(&gameservercriti);
		InitializeCriticalSection(&g_critERROR_LOG);
		
		unsigned int ThreadID;

		hThread = (HANDLE)_beginthreadex(
			NULL,
			0,
			(unsigned(__stdcall*)(void*))MainThreadProc,
			NULL,
			0,
			&ThreadID);

		CloseHandle(hThread);
		// Set periodic timers
		SetTimer(hMainDlg, 2, 30000, NULL);   // 30 sec - Server list refresh
		SetTimer(hMainDlg, 3, 3000, NULL);    // 3 sec - UI status update
		SetTimer(hMainDlg, 4, 180000, NULL);  // 3 min - Timeout check
		SetTimer(hMainDlg, 5, 5000, NULL);    // 5 sec - Join server status
		SetTimer(hMainDlg, 7, 3000, NULL);    // 3 sec - Write server info
		SetTimer(hMainDlg, 8, 3000, NULL);    // 3 sec - Multicast update

		InitializeCriticalSection(&g_critCSINFO_LIST);

		if (g_PerfMonEx.Initialize())
		{
			g_iPERF_IDX_CPU = g_PerfMonEx.AddCounter(CNTR_CPU);
			g_iPERF_IDX_MEM = g_PerfMonEx.AddCounter(CNTR_MEMINUSE_BYTES);
		}
		else
		{
			MessageBox(hWnd, "CPerfMonEx Initialize Failed", "ERROR", MB_ICONERROR | MB_OK);
		}

		CheckNShowPublicIPAddress();

		g_GameServerInfoSock.InitializeWzTcpSock();

		g_GameServerInfoSock.Initialize(hWnd, 0, (void(__cdecl*)(unsigned int, void*, unsigned __int8, unsigned __int8, char*, int))GSINFO_PACKETPROC_RECV, GSINFO_PACKETPROC_OTHER);

		if (g_GameServerInfoSock.Accept(55558))
		{
			g_LogProc.LogAddFunc("TCP Recv Socket Initialize OK.  Port : %d", 55558);
		}

		if (g_WhatUpDummyServer.Start(hWnd, 55560))
		{
			g_LogProc.LogAddFunc("What's Up DUMMY OK Port : %d", 55560);
		}
		else
		{
			g_LogProc.LogAddFunc("What's Up DUMMY FAILED Port : %d", 55560);
		}
		// Multicast callback registration
		g_lpMultiCastRecvNotifyProc = (int)MULTICAST_RECVNOTIFY_PROC;
		g_lpMultiCastErrorNotifyProc = (int)MULTICAST_ERRORNOTIFY_PROC;

		// Log startup
		g_LogFileError.Output("<<================== Connect Server Starting ==================>>\n");

		return TRUE;
	}

	case WM_DESTROY:
	{
		CTrayCtl.DeleteTrayIcon();

		DeleteCriticalSection(&gameservercriti);
		DeleteCriticalSection(&g_critERROR_LOG);
		DeleteCriticalSection(&g_critCSINFO_LIST);
		g_LogFileError.Output("<<================== Connect Server Closing ==================>>\n");

		ExitProcess(0);

		return TRUE;
	}
	case WM_CLOSE:
	{
		if (MessageBox(hWnd, "Are you sure to terminate server?", "Ask Terminate Server", MB_YESNO | MB_ICONQUESTION) != IDYES)
		{
			return TRUE;
		}

		g_bDoTerminateLoopThread = TRUE;
		g_GameServerInfoSock.EndWzTcpSock();
		EndDialog(hWnd, 0);
		return TRUE;
	}
	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case 1:
			return TRUE;

		case IDC_CHECK_PUBLIC_IP:
		{
			hEdit = GetDlgItem(hWnd, IDC_CHECK_PUBLIC_IP);
			SendMessage(hEdit, EM_SETSEL, 0, 0);

			if (!hPbIpSelectDlg)
			{
				hPbIpSelectDlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_PUBLIC_IP_DIALOG), hWnd, PbIpSelectDlgProc, 0);
			}

			return TRUE;
		}

		case IDC_BTN_RELEASE_RESTRICTED_IP:
		{
			g_CsIpAddrMng.ReleaseRestriectedIpAddr();
			SetDlgItemInt(hWnd, IDC_EDIT_PUBLIC_IP, g_CsIpAddrMng.GetRestrictedIpIpAddrCount(), FALSE);
			return TRUE;
		}

		case 8001:
		case 0x9C4A:
		case ID_TOOL_REFRESH:     // 40030
		{
			ShowGameServerListToTreeView();
			return TRUE;
		}

		case 8002:
		case 0x9C50:
		case ID_TOOL_RELOAD:      // 40031
		{
			if (!IsWindow(hReloadDataDlg))
			{
				hReloadDataDlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_RELOAD_DIALOG), hWnd, ReloadDataDlgProc, 0);
			}
			return TRUE;
		}

		case 8003:
		case 0x9C57:
		case ID_TOOL_LOCAL_ONLY:  // 40033
		{
			if (g_bLocalNetworkConnectOnly)
			{
				if (MessageBox(hWnd,
					"Are You Sure to Turn Off Local Connect Option ?",
					"Ask Change Local Connect Only Option", MB_YESNO | MB_ICONQUESTION) == IDNO)
				{
					return TRUE;
				}
			}
			else
			{
				if (MessageBox(hWnd,
					"Are You Sure to Turn On Local Connect Option ?\nAll Current Connection will be lost.",
					"Ask Change Local Connect Only Option", MB_YESNO | MB_ICONQUESTION) == IDNO)
				{
					return TRUE;
				}
			}

			g_bLocalNetworkConnectOnly = !g_bLocalNetworkConnectOnly;

			g_LogProc.LogAddFunc("Local Network Connect Only : %s", g_bLocalNetworkConnectOnly ? "ON" : "OFF");

			// Update toolbar button state
			//SendMessageA(hToolBar, TB_CHECKBUTTON, ID_TOOL_LOCAL_ONLY, MAKELPARAM(g_bLocalNetworkConnectOnly, 0));
			SendMessage(hToolBar, TB_CHANGEBITMAP, ID_TOOL_LOCAL_ONLY, g_bLocalNetworkConnectOnly ? 8 : 3);

			return TRUE;
		}

		case 8004:
		case 0x9C4C:
		case 0x9C51:
		case ID_TOOL_TERMINATE:   // 40032
		{
			if (MessageBox(hWnd, "Are You Sure to Terminate Connect Server ?", "Ask Terminate Server", MB_YESNO | MB_ICONQUESTION) != IDYES)
			{
				return TRUE;
			}
			EndDialog(hWnd, 0);
			return TRUE;
		}

		case 8005:
		case 0x9C56:
		case ID_TOOL_ERROR_LOG:   // 40034
		{
			if (IsWindow(hErrorLogDlg))
			{
				ShowWindow(hErrorLogDlg, SW_SHOW);
			}
			else
			{
				hErrorLogDlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_ERROR_LOG_DIALOG), hWnd, ErrorLogDlgProc, 0);
			}

			EndErrorLogAlarm();

			return TRUE;
		}

		case 8006:
		case ID_MENU_MULTICAST_CS_INFO:
		{
			if (!hMcIpSelectDlg)
			{
				hMcIpSelectDlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_MULTICAST_DIALOG), hWnd, McIpSelectDlgProc, 0);
			}
			return TRUE;
		}

		case ID_MENU_SHOW_CS_UI:
		{
			ShowWindow(hWnd, SW_SHOW);
			SetForegroundWindow(hWnd);
			SetFocus(hWnd);
			return TRUE;
		}

		case ID_MENU_ABOUT:
		{
			hAboutDlg = CreateDialogParam(g_hInst, MAKEINTRESOURCE(IDD_ABOUT_DIALOG), hWnd, AboutDlgProc, 0);
			return TRUE;
		}
		}

		return TRUE;
	}

	case WM_NOTIFY:
	{
		LPNMHDR lpHdr = (LPNMHDR)lParam;

		if (lpHdr->hwndFrom == GetDlgItem(hWnd, IDC_TREE_SERVER_LIST))
		{
			CTreeCtl.TreeNotify(wParam, (LPARAM)lpHdr);
		}

		if (lpHdr->code != TTN_NEEDTEXT)
		{
			return TRUE;
		}

		LPTOOLTIPTEXT lpText = (LPTOOLTIPTEXT)lParam;

		switch ((UINT)wParam)
		{
			//case 0x1F41:
		case ID_TOOL_REFRESH:
			lpText->lpszText = "Refresh Game Server List";
			break;

			//case 0x1F42:
		case ID_TOOL_RELOAD:
			lpText->lpszText = "Reload Connect Server Data Files";
			break;

			//case 0x1F43:
		case ID_TOOL_TERMINATE:
			lpText->lpszText = "Terminate Connect Server";
			break;

			//case 0x1F44:
		case ID_TOOL_LOCAL_ONLY:
			lpText->lpszText = g_bLocalNetworkConnectOnly ? "Local Network Connect Only ON" : "Local Network Connect Only OFF";
			break;

			//case 0x1F45:
		case ID_TOOL_ERROR_LOG:
			lpText->lpszText = "Show Fatal Error Log List";
			break;

		case 0x1F46:
		case ID_TOOL_MULTICAST:
			lpText->lpszText = g_bMultiCastOn ? "Multicasting CS Info ON" : "Multicasting CS Info OFF";
			break;
		}

		return TRUE;
	}

	case WM_TIMER:
	{
		switch (wParam)
		{
		case 1:
		{
			if (g_nICON_IMAGEID % 2)
			{
				SendMessage(hToolBar, TB_CHANGEBITMAP, ID_TOOL_ERROR_LOG, 7);
				CTrayCtl.SetTrayIcon(IDI_ICON_TEXT_CS_RED, "ConnectServer Error");
			}
			else
			{
				SendMessage(hToolBar, TB_CHANGEBITMAP, ID_TOOL_ERROR_LOG, 6);
				CTrayCtl.SetTrayIcon(IDI_ICON_TEXT_CS_YELLOW, "ConnectServer Error");
			}
			++g_nICON_IMAGEID;
			return TRUE;
		}

		case 2:
		{
			ShowGameServerListToTreeView();
			return TRUE;
		}

		case 3:
		{
			wsprintf(String, "%d / %d", g_ObjIndexQ.size(), MAX_CONNECTOBJECT);
			SetDlgItemText(hWnd, IDC_EDIT_STACK_SIZE, String);
			wsprintf(String, "%d", g_iSockConnNum);
			SetDlgItemText(hWnd, IDC_EDIT_USER_COUNT, String);

			CheckLocalNetworkConnectOnly();
			SetDlgItemInt(hWnd, IDC_EDIT_PUBLIC_IP, g_CsIpAddrMng.GetRestrictedIpIpAddrCount(), FALSE);

			return TRUE;
		}

		case 4:
		{
			gObjCheckTimeOut();
			g_CsIpAddrMng.CheckIpAddrList();

			return TRUE;
		}

		case 5:
		{
			SetDlgItemText(hWnd, IDC_EDIT_JOIN_SERVER_LIVE, g_bJoinServerLive ? "ON" : "OFF");
			return TRUE;
		}

		case 6:
		{
			WriteGameServerInfo();
			return TRUE;
		}

		case 7:
		{
			MC_SEND_ConnectServerInfo();
			if (MakeCcsShareInfoBuffer())
			{
				MC_SEND_CcsShareInfo();
			}
			CFileServerInfoMake_CCS();

			return TRUE;
		}

		case 8:
		{
			IpSelectDlg_ClearListView();
			if (!g_bMultiCastOn)
			{
				return TRUE;
			}
			MC_ShowConnectServerList();
			return TRUE;
		}
		}

		return TRUE;
	}

	case WM_LBUTTONDOWN:
		return TRUE;

	case WM_LBUTTONDBLCLK:
		return TRUE;

	case WM_TRAYICON:
	{
		CTrayCtl.TrayNotify(hWnd, wParam, lParam);
		return TRUE;
	}
	}

	return FALSE;
}
// =========================================================================
// ReloadDataDlgProc - Reload Data Files Dialog
// =========================================================================
INT_PTR CALLBACK ReloadDataDlgProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
		// Disable "Connect Server List" button (IDC_BTN_RELOAD_CONNECTSERVER = 1004)
		EnableWindow(GetDlgItem(hWndDlg, IDC_BTN_RELOAD_CONNECTSERVER), FALSE);
		return TRUE;

	case WM_CLOSE:
	case WM_DESTROY:
		hReloadDataDlg = NULL;
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BTN_RELOAD_GAMESERVER:  // 1002
			if (MessageBoxA(hWndDlg, "Are You Sure to Reload Game Server List ?", "Ask Reload Data", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				ReadServerList();
				ShowGameServerListToTreeView();
			}
			DestroyWindow(hWndDlg);
			return TRUE;

		case IDC_BTN_RELOAD_PATCHSERVER:  // 1003
			if (MessageBoxA(hWndDlg, "Warning!! It could cause some troubles to User for a few seconds.\n\nAre You Sure to Reload Patch Server Info ?", "Ask Reload Data", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				ReadServerInfo();
			}
			DestroyWindow(hWndDlg);
			return TRUE;

		case IDC_BTN_RELOAD_CONNECTSERVER:  // 1004 (disabled, but handle anyway)
			if (MessageBoxA(hWndDlg, "Warning!! It could cause some troubles to User for a few seconds.\n\nAre You Sure to Reload Connect Server List ?", "Ask Reload Data", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				ReadConnServerList();
			}
			DestroyWindow(hWndDlg);
			return TRUE;

		case IDCANCEL:
		case IDOK:
			DestroyWindow(hWndDlg);
			return TRUE;
		}
		return TRUE;
	}
	return FALSE;
}

// =========================================================================
// Error Log Alarm Functions
// =========================================================================
void StartErrorLogAlarm()
{
	g_nICON_IMAGEID = 0;
	SetTimer(hMainDlg, 1, 1000, NULL);  // 1 second blink
}

void EndErrorLogAlarm()
{
	KillTimer(hMainDlg, 1);
	SendMessageA(hToolBar, TB_CHECKBUTTON, ID_TOOL_ERROR_LOG, MAKELPARAM(0, 0));
	CTrayCtl.SetTrayIcon(IDI_ICON_STATE_ONLINE, "Connect Server OK.");
}

// =========================================================================
// ErrorLogDlgProc - Error Log Display Dialog
// =========================================================================
INT_PTR CALLBACK ErrorLogDlgProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	HICON hIcon;
	RECT rcClient, rcBtn;
	HWND hList;
	HWND hBtn;

	int nListWidth;
	int nListHeight;
	int nBtnX;
	int nBtnY;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		// Set dialog icon
		hIcon = LoadIconA(g_hInst, MAKEINTRESOURCEA(IDI_ICON_APP_MAIN));
		SendMessageA(hWndDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

		// Trigger WM_SIZE to layout controls
		SendMessageA(hWndDlg, WM_SIZE, 0, 0);
		return TRUE;

	case WM_CLOSE:
		ShowWindow(hWndDlg, SW_HIDE);
		return TRUE;

	case WM_SIZE:
	{
		GetClientRect(hWndDlg, &rcClient);

		// Resize list box (IDC_LIST_ERROR_LOG = 1007)
		hList = GetDlgItem(hWndDlg, IDC_LIST_ERROR_LOG);
		nListWidth = rcClient.right - 20;
		nListHeight = rcClient.bottom - 65;
		MoveWindow(hList, 10, 10, nListWidth, nListHeight, TRUE);

		// Center "Clear Error Log" button
		GetClientRect(GetDlgItem(hWndDlg, IDC_BTN_CLEAR_ERROR_LOG), &rcBtn);
		hBtn = GetDlgItem(hWndDlg, IDC_BTN_CLEAR_ERROR_LOG);
		nBtnX = (rcClient.right - rcBtn.right) / 2;
		nBtnY = rcClient.bottom - 45;
		SetWindowPos(hBtn, NULL, nBtnX, nBtnY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
	}
	return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_BTN_CLEAR_ERROR_LOG:  // 1008
		{
			hList = GetDlgItem(hWndDlg, IDC_LIST_ERROR_LOG);
			SendMessageA(hList, LB_RESETCONTENT, 0, 0);
			// Clear underlying log storage here
		}
		return TRUE;

		case IDCANCEL:
			ShowWindow(hWndDlg, SW_HIDE);
			return TRUE;
		}
		return TRUE;
	}
	return FALSE;
}

// =========================================================================
// AboutDlgProc - About Dialog Window Procedure
// =========================================================================
INT_PTR CALLBACK AboutDlgProc(HWND hWndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	//static const char* a20060123 = "20060123";
	static const char* aCountryVietnam = "VIETNAM";
	static const char* aWebzenLogo = "Webzen Mu Connect Server";

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		// Set version text (IDC_STATIC_VERSION = 1003)
		char szVersion[64];
		_snprintf(szVersion, sizeof(szVersion), "Version : %s", CONNECT_SERVER_VERSION);
		SetDlgItemTextA(hWndDlg, IDC_STATIC_VERSION, szVersion);

		// Set country text (IDC_STATIC_COUNTRY = 1004)
		SetDlgItemTextA(hWndDlg, IDC_STATIC_COUNTRY, aCountryVietnam);

		return TRUE;
	}

	case WM_CLOSE:
	case WM_DESTROY:
		hAboutDlg = NULL;
		return TRUE;

	case WM_PAINT:
	{
		// Draw Webzen logo bitmap with transparency
		// IDB_BITMAP_WEBZEN_LOGO = 140 = 0x8D
		PAINTSTRUCT ps;
		HDC hDC = BeginPaint(hWndDlg, &ps);
		HDC hMemDC = CreateCompatibleDC(hDC);

		HBITMAP hBmp = LoadBitmapA(g_hInst, MAKEINTRESOURCEA(IDB_BITMAP_WEBZEN_LOGO));
		if (hBmp)
		{
			HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);

			// TransparentBlt: source has magenta (0xFF00FF) as transparent color
			// Original: TransparentBlt(hDC, 123, 52, 140, 80, hMemDC, 0, 0, 106, 68, 0xFFFFFF)
			// Note: Last param is actually the transparent color key (0xFF00FF for magenta)
			TransparentBlt(hDC, 123, 52, 140, 80, hMemDC, 0, 0, 106, 68, 0x00FF00FF);

			SelectObject(hMemDC, hOldBmp);
			DeleteObject(hBmp);
		}

		DeleteDC(hMemDC);
		EndPaint(hWndDlg, &ps);
		return TRUE;
	}

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			DestroyWindow(hWndDlg);
			return TRUE;
		}
		return TRUE;

	case WM_LBUTTONDOWN:
		// Click anywhere to close (original behavior)
		DestroyWindow(hWndDlg);
		return TRUE;
	}

	return FALSE;
}

// =========================================================================
// CheckNShowPublicIPAddress - Enumerate and filter local IPs
// =========================================================================
void CheckNShowPublicIPAddress()
{
	static char g_szPB_IPADDR[32] = { 0 };

	memset(g_szPB_IPADDR, 0, sizeof(g_szPB_IPADDR));

	// Enumerate local IP addresses
	int nCount = g_CsInfoMultiCast.DoEnumIpAddress();
	if (nCount <= 0)
		goto LABEL_SET_DEFAULT;

	// Search for public IP (filter out localhost/127.x and 10.x/192.168.x)
	for (int i = 0; i < nCount; ++i)
	{
		char szIP[32] = { 0 };
		if (!g_CsInfoMultiCast.GetEnummedIpAddress(i, szIP))
			continue;

		// Filter logic from original:
		// Skip if starts with "10." or "192.168." or "127."
		if (szIP[0] == '1' && szIP[1] == '0' && szIP[2] == '.')
			continue;
		if (szIP[0] == '1' && szIP[1] == '9' && szIP[2] == '2' && szIP[3] == '.')
			continue;
		if (szIP[0] == '1' && szIP[1] == '2' && szIP[2] == '7' && szIP[3] == '.')
			continue;
		if (szIP[0] == '1' && szIP[1] == '7' && szIP[2] == '2' && szIP[3] == '.')
			continue;

		// Found a candidate public IP
		strncpy(g_szPB_IPADDR, szIP, sizeof(g_szPB_IPADDR) - 1);
		g_szPB_IPADDR[sizeof(g_szPB_IPADDR) - 1] = '\0';
		break;
	}

LABEL_SET_DEFAULT:
	// Display in main dialog (IDC_EDIT_PUBLIC_IP = 1014)
	if (hMainDlg && g_szPB_IPADDR[0])
		SetDlgItemTextA(hMainDlg, IDC_CHECK_PUBLIC_IP, g_szPB_IPADDR);
	else
		SetDlgItemTextA(hMainDlg, IDC_CHECK_PUBLIC_IP, "N/A");
}

// =========================================================================
// McIpSelectDlgProc - Multicast IP Selection Dialog
// =========================================================================
INT_PTR CALLBACK McIpSelectDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		hMcIpSelectDlg = hDlg;

		// Initialize ListView (IDC_LIST_CS_INFO = 1013)
		IpSelectDlg_InitListViewCtrl();

		// Populate local IP list (IDC_LIST_LOCAL_IP = 1009)
		int nCount = g_CsInfoMultiCast.DoEnumIpAddress();
		if (nCount > 0)
		{
			for (int i = 0; i < nCount; ++i)
			{
				char szIP[32] = { 0 };
				if (g_CsInfoMultiCast.GetEnummedIpAddress(i, szIP))
				{
					HWND hList = GetDlgItem(hDlg, IDC_LIST_LOCAL_IP);
					SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)szIP);
				}
			}
		}

		// Enable/disable controls based on multicast state
		if (g_bMultiCastOn)
		{
			EnableWindow(GetDlgItem(hDlg, IDC_LIST_LOCAL_IP), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_BTN_START_MULTICAST), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_BTN_END_MULTICAST), TRUE);
			MC_ShowConnectServerList();
		}
		else
		{
			EnableWindow(GetDlgItem(hDlg, IDC_LIST_LOCAL_IP), TRUE);
			EnableWindow(GetDlgItem(hDlg, IDC_BTN_START_MULTICAST), TRUE);
			EnableWindow(GetDlgItem(hDlg, IDC_BTN_END_MULTICAST), FALSE);
		}

		return TRUE;
	}

	case WM_CLOSE:
	{
		hMcIpSelectDlg = NULL;
		DestroyWindow(hDlg);
		return TRUE;
	}

	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDC_BTN_START_MULTICAST:  // 1010
		{
			HWND hList = GetDlgItem(hDlg, IDC_LIST_LOCAL_IP);
			LRESULT nSel = SendMessageA(hList, LB_GETCURSEL, 0, 0);

			if (nSel == LB_ERR)
			{
				MessageBoxA(hDlg, "Please Choose IP to Join MultiCast Group First.", "ERROR", MB_ICONERROR | MB_OK);
				return TRUE;
			}

			// Get selected IP
			SendMessageA(hList, LB_GETTEXT, (WPARAM)nSel, (LPARAM)g_szMC_IPADDR);

			// Confirm with user
			char szConfirm[256];
			wsprintfA(szConfirm, "Are You Sure to Use IP Address '%s' to Join MultiCast Group ?", g_szMC_IPADDR);

			if (MessageBoxA(hDlg, szConfirm, "Ask Start MultiCasting", MB_YESNO | MB_ICONQUESTION) != IDYES)
				return TRUE;

			// Start multicast
			if (!g_CsInfoMultiCast.StartMultiCast(g_szMC_IPADDR))
				return TRUE;

			g_LogProc.LogAddFuncColor(3, "Starting Connect Server Information MultiCast. IP:%s", g_szMC_IPADDR);

			hMcIpSelectDlg = NULL;
			g_bDoMultiCast = TRUE;
			g_bMultiCastOn = TRUE;
			g_bMULTICAST_LIST_REMAKE = TRUE;

			// Update toolbar button - USE TB_CHANGEBITMAP with bitmap index 9
			SendMessageA(hToolBar, TB_CHANGEBITMAP, ID_TOOL_MULTICAST, 9);

			DestroyWindow(hDlg);
		}
		return TRUE;

		case IDC_BTN_END_MULTICAST:  // 1011
		{
			if (MessageBoxA(hDlg, "Are You Sure to End CS Info MultiCasting ?", "Ask End MultiCasting", MB_YESNO | MB_ICONQUESTION) != IDYES)
				return TRUE;

			hMcIpSelectDlg = NULL;
			g_bDoMultiCast = FALSE;
			g_bMultiCastOn = FALSE;

			g_CsInfoMultiCast.EndMultiCast();

			// Update toolbar button - USE TB_CHANGEBITMAP with bitmap index 5
			SendMessageA(hToolBar, TB_CHANGEBITMAP, ID_TOOL_MULTICAST, 5);

			DestroyWindow(hDlg);
		}
		return TRUE;

		case IDCANCEL:  // 2
		{
			// Original does NOT set hMcIpSelectDlg = NULL here!
			DestroyWindow(hDlg);
			return TRUE;
		}
		}
		return TRUE;
	}
	}

	return FALSE;
}

// =========================================================================
// PbIpSelectDlgProc - Public IP Selection Dialog
// =========================================================================
INT_PTR CALLBACK PbIpSelectDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char szIP[32] = { 0 };

	switch (uMsg)
	{
	case WM_INITDIALOG:
	{
		hPbIpSelectDlg = hDlg;

		// Initialize ListView
		IpSelectDlg_InitListViewCtrl();

		// Populate IP list (IDC_LIST_PUBLIC_IP = 1015)
		int nCount = g_CsInfoMultiCast.DoEnumIpAddress();
		if (nCount > 0)
		{
			for (int i = 0; i < nCount; ++i)
			{
				if (g_CsInfoMultiCast.GetEnummedIpAddress(i, szIP))
				{
					HWND hList = GetDlgItem(hDlg, IDC_LIST_PUBLIC_IP);
					SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)szIP);
				}
			}
		}

		return TRUE;
	}

	case WM_CLOSE:  // 16
	{
		hPbIpSelectDlg = NULL;
		DestroyWindow(hDlg);
		return TRUE;
	}

	case WM_COMMAND:
	{
		switch (LOWORD(wParam))
		{
		case IDC_BTN_SELECT_IP:  // 1016
		{
			HWND hList = GetDlgItem(hDlg, IDC_LIST_PUBLIC_IP);
			LRESULT nSel = SendMessageA(hList, LB_GETCURSEL, 0, 0);

			if (nSel == LB_ERR)
			{
				MessageBoxA(hDlg, "Please Choose IP to Set as Public IP Address", "ERROR", MB_ICONERROR | MB_OK);
				return TRUE;
			}

			// Get selected IP
			char szSelectedIP[32] = { 0 };
			SendMessageA(hList, LB_GETTEXT, (WPARAM)nSel, (LPARAM)szSelectedIP);

			// Confirm with user
			char szConfirm[256];
			wsprintfA(szConfirm, "Are You Sure to Use IP Address '%s' to Public IP of CS ?", szSelectedIP);

			if (MessageBoxA(hDlg, szConfirm, "Ask Setting Public IP Address Info", MB_YESNO | MB_ICONQUESTION) == IDYES)
			{
				// Copy exactly 32 bytes (matching qmemcpy)
				memcpy(g_szPB_IPADDR, szSelectedIP, 0x20);

				// Update main dialog display
				if (hMainDlg)
					SetDlgItemTextA(hMainDlg, IDC_CHECK_PUBLIC_IP, g_szPB_IPADDR);

				hPbIpSelectDlg = NULL;
				DestroyWindow(hDlg);
			}
			// If user clicked No, window stays open - do nothing

			return TRUE;
		}

		case IDC_BTN_CANCEL_IP:  // 1017
		{
			hPbIpSelectDlg = NULL;
			DestroyWindow(hDlg);
			return TRUE;
		}

		case IDCANCEL:  // 2
		{
			// Does NOT set hPbIpSelectDlg = NULL!
			DestroyWindow(hDlg);
			return TRUE;
		}
		}
		return TRUE;
	}
	}

	return FALSE;
}

// =========================================================================
// IpSelectDlg_InitListViewCtrl - Initialize ListView for IP Selection
// =========================================================================
void IpSelectDlg_InitListViewCtrl()
{
	// Reset item counter
	g_iLV_ItemNo = 0;

	// ListView control ID: IDC_LIST_CS_INFO = 1013
	HWND hList = GetDlgItem(hMcIpSelectDlg, IDC_LIST_CS_INFO);
	if (!hList) return;

	// Set extended styles: full-row select only (NOT grid lines!)
	LRESULT dwExStyle = SendMessageA(hList, LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0);
	dwExStyle |= LVS_EX_FULLROWSELECT;
	SendMessageA(hList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, dwExStyle);

	// Add columns (LVM_INSERTCOLUMN = 0x101B)
	LV_COLUMNA lvc = { 0 };
	lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM | LVCF_FMT;  // 0x0F
	lvc.fmt = LVCFMT_LEFT;  // 0

	// Column 0: CS Multicast IP (120px)
	lvc.pszText = "CS Multicast IP";
	lvc.cx = 120;
	lvc.iSubItem = 0;
	SendMessageA(hList, LVM_INSERTCOLUMNA, 0, (LPARAM)&lvc);

	// Column 1: CS Public IP (120px)
	lvc.pszText = "CS Public IP";
	lvc.cx = 120;
	lvc.iSubItem = 1;
	SendMessageA(hList, LVM_INSERTCOLUMNA, 1, (LPARAM)&lvc);

	// Column 2: Current Connect (150px)
	lvc.pszText = "Current Connect";
	lvc.cx = 150;
	lvc.iSubItem = 2;
	SendMessageA(hList, LVM_INSERTCOLUMNA, 2, (LPARAM)&lvc);

	// Column 3: CPU Usage (120px)
	lvc.pszText = "CPU Usage";
	lvc.cx = 120;
	lvc.iSubItem = 3;
	SendMessageA(hList, LVM_INSERTCOLUMNA, 3, (LPARAM)&lvc);

	// Column 4: Mem Usage (120px)
	lvc.pszText = "Mem Usage";
	lvc.cx = 120;
	lvc.iSubItem = 4;
	SendMessageA(hList, LVM_INSERTCOLUMNA, 4, (LPARAM)&lvc);

	// Additional messages from original (purpose unclear, but present)
	// These might be setting default item or preparing for item insertion
	LV_ITEMA lvi = { 0 };
	lvi.mask = LVIF_TEXT;
	lvi.pszText = "";
	lvi.iItem = 4;  // wParam from last column
	SendMessageA(hList, LVM_SETITEMA, (WPARAM)4, (LPARAM)&lvi);
	SendMessageA(hList, LVM_SETITEMTEXTA, (WPARAM)4, (LPARAM)&lvi);

	// Get and set column order array
	int nOrderArray[32] = { 0 };  // Buffer for column order
	SendMessageA(hList, LVM_GETCOLUMNORDERARRAY, 0, (LPARAM)nOrderArray);

	// Modify the order (original ORs with 0x20 on the LOBYTE of return value)
	// This is likely setting a flag or modifying the column order
	LRESULT lResult = SendMessageA(hList, LVM_GETCOLUMNORDERARRAY, 0, 0);
	lResult |= 0x20;
	SendMessageA(hList, LVM_SETCOLUMNORDERARRAY, 0, lResult);
}

// =========================================================================
// IpSelectDlg_InsertListViewCtrl - Add Row to ListView
// =========================================================================
void IpSelectDlg_InsertListViewCtrl(in_addr addrMulticast, in_addr addrPublic, int nConnectCount, int nCPU, int nMem)
{
	HWND hList = GetDlgItem(hMcIpSelectDlg, IDC_LIST_CS_INFO);
	if (!hList) return;

	// Convert IPs to strings using strcpy (not strncpy)
	char szMulticast[32] = { 0 };
	char szPublic[32] = { 0 };

	char* pMulti = inet_ntoa(addrMulticast);
	strcpy(szMulticast, pMulti ? pMulti : "N/A");

	char* pPublic = inet_ntoa(addrPublic);
	strcpy(szPublic, pPublic ? pPublic : "N/A");

	// Setup LV_ITEM for insertion
	LV_ITEMA lvi = { 0 };
	lvi.mask = LVIF_TEXT;
	lvi.iItem = g_iLV_ItemNo;
	lvi.iSubItem = 0;
	lvi.pszText = szMulticast;

	// Insert the item
	SendMessageA(hList, LVM_INSERTITEMA, 0, (LPARAM)&lvi);

	// SubItem 1: Public IP
	lvi.iSubItem = 1;
	lvi.pszText = szPublic;
	SendMessageA(hList, LVM_SETITEMTEXTA, g_iLV_ItemNo, (LPARAM)&lvi);

	// SubItem 2: Connect Count (no units, just number)
	char szBuf[32];
	wsprintfA(szBuf, "%d", nConnectCount);
	lvi.iSubItem = 2;
	lvi.pszText = szBuf;
	SendMessageA(hList, LVM_SETITEMTEXTA, g_iLV_ItemNo, (LPARAM)&lvi);

	// SubItem 3: CPU Usage (no % sign)
	wsprintfA(szBuf, "%d", nCPU);
	lvi.iSubItem = 3;
	lvi.pszText = szBuf;
	SendMessageA(hList, LVM_SETITEMTEXTA, g_iLV_ItemNo, (LPARAM)&lvi);

	// SubItem 4: Memory Usage (no MB)
	wsprintfA(szBuf, "%d", nMem);
	lvi.iSubItem = 4;
	lvi.pszText = szBuf;
	SendMessageA(hList, LVM_SETITEMTEXTA, g_iLV_ItemNo, (LPARAM)&lvi);

	++g_iLV_ItemNo;
}

// =========================================================================
// IpSelectDlg_ClearListView - Clear All ListView Items
// =========================================================================
void IpSelectDlg_ClearListView()
{
	HWND hList = GetDlgItem(hMcIpSelectDlg, IDC_LIST_CS_INFO);
	if (hList)
	{
		ListView_DeleteAllItems(hList);  // LVM_DELETEALLITEMS = 0x1009
	}
	g_iLV_ItemNo = 0;
}

// =========================================================================
// MakeCcsShareInfoBuffer - Build CCS Share Info Buffer
// =========================================================================
BOOL MakeCcsShareInfoBuffer()
{
	EnterCriticalSection(&gameservercriti);

	int shareCount = 0;

	PMSG_CCS_INFOLIST* lpEntry = &g_cCCS_INFOSHARE_BUF.Entry[0];

	GameServerList_Struct* lpServer = GameServerList;

	while ((int)lpServer < (int)ClientVersion)
	{
		if (lpServer->Live == TRUE)
		{
			lpEntry->ServerCode = lpServer->ServerCode;
			lpEntry->ServerType = lpServer->ServerType;
			lpEntry->MaxUserCount = lpServer->MaxUserCount;
			lpEntry->PCbangCount = lpServer->MaxUserCount - lpServer->UserCount;
			lpEntry->UserCount = lpServer->UserCount;
			lpEntry->ServerFlag = lpServer->ServerFlag;

			++shareCount;
			++lpEntry;
		}

		++lpServer;
	}

	g_cCCS_INFOSHARE_BUF.JoinServerCount = g_JoinServerRecvQCount;
	g_cCCS_INFOSHARE_BUF.ShareCount = shareCount;

	g_cCCS_INFOSHARE_BUF.c		= PMHC_WORD;
	g_cCCS_INFOSHARE_BUF.sizeH	= SET_NUMBERH(shareCount);
	g_cCCS_INFOSHARE_BUF.sizeL	= SET_NUMBERL(shareCount);
	g_cCCS_INFOSHARE_BUF.head	= 0x03;

	LeaveCriticalSection(&gameservercriti);

	return (shareCount > 0);
}

// =========================================================================
// InitDlgControls - Initialize Toolbar and Dialog Controls
// =========================================================================
void InitDlgControls(HWND hWnd)
{
	// Load toolbar bitmap (0x7A = 122)
	LoadBitmapA(g_hInst, MAKEINTRESOURCEA(0x7A));

	// Create toolbar
	TBBUTTON Buttons = { 0 };
	hToolBar = CreateToolbarEx(hWnd,
		WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT | TBSTYLE_TOOLTIPS | CCS_TOP | CCS_NODIVIDER,
		0x1F40,              // Toolbar ID = 8000
		11,                  // Max buttons
		g_hInst,
		IDB_BITMAP_TOOLBAR,                // Bitmap resource ID
		&Buttons,            // Button array
		0,                   // Initial button count = 0
		32, 32, 32, 32,     // Button sizes
		sizeof(TBBUTTON));

	if (hToolBar)
	{
		int nBitmapIdx = 0;

		for (int nBtnIdx = 0; nBtnIdx < 11; ++nBtnIdx)
		{
			// Set common button properties
			Buttons.fsState = TBSTATE_ENABLED;
			Buttons.iString = 0;

			if (nBtnIdx == 1 || nBtnIdx == 3 || nBtnIdx == 5 || nBtnIdx == 7 || nBtnIdx == 9)
			{
				// Separator
				Buttons.idCommand = 0;
				Buttons.fsStyle = TBSTYLE_SEP;
			}
			else
			{
				// Regular button
				Buttons.iBitmap = nBitmapIdx;
				Buttons.fsStyle = TBSTYLE_BUTTON;
				Buttons.idCommand = nBitmapIdx + ID_TOOL_REFRESH;  // Start from 8001
				++nBitmapIdx;
			}

			SendMessageA(hToolBar, TB_ADDBUTTONS, 1, (LPARAM)&Buttons);
		}
	}

	// Auto-size toolbar
	SendMessageA(hToolBar, TB_AUTOSIZE, 0, 0);

	// Set multicast button state to unchecked (lParam = 0, not MAKELPARAM)
	HWND hDlgItem = GetDlgItem(hMainDlg, 8000);
	SendMessageA(hDlgItem, TB_CHECKBUTTON, IDC_CHECK_PUBLIC_IP, 0);  // Just 0, not MAKELPARAM

	// Disable Public IP edit field (IDC_CHECK_PUBLIC_IP = 1014)
	HWND hEdit = GetDlgItem(hMainDlg, IDC_CHECK_PUBLIC_IP);
	EnableWindow(hEdit, FALSE);

	// Update window title with hardcoded version string
	char szTitle[256];
	GetWindowTextA(hWnd, szTitle, sizeof(szTitle));

	SYSTEMTIME st;
	GetLocalTime(&st);

	// Use wsprintfA directly on szTitle (overwrites it)
	wsprintfA(szTitle,
		"%s (Version : %s, Start Date : %d-%d-%d %02d:%02d:%02d.%03d)",
		szTitle,
		CONNECT_SERVER_VERSION, 
		st.wYear, st.wMonth, st.wDay,
		st.wHour, st.wMinute, st.wSecond,
		st.wMilliseconds);

	SetWindowTextA(hWnd, szTitle);

	// Set window icon (0x67 = 103)
	HICON hIcon = LoadIconA(g_hInst, MAKEINTRESOURCEA(IDI_ICON_APP_MAIN));
	SendMessageA(hMainDlg, WM_SETICON, ICON_BIG, (LPARAM)hIcon);

	// Refresh system tray icons
	HWND hShell = FindWindowExA(NULL, NULL, "Shell_TrayWnd", NULL);
	if (hShell)
	{
		HWND hNotify = FindWindowExA(hShell, NULL, "TrayNotifyWnd", NULL);
		if (hNotify)
		{
			HWND hTrayBar = FindWindowExA(hNotify, NULL, "ToolbarWindow32", NULL);
			if (hTrayBar)
			{
				RECT rc;
				GetClientRect(hTrayBar, &rc);

				for (int y = 0; y < rc.bottom - rc.top; y += 8)
				{
					for (int x = 0; x < rc.right - rc.left; x += 8)
					{
						SendMessageA(hTrayBar, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
					}
				}
			}
		}
	}
}

// =========================================================================
// MainThreadProc - Main Worker Thread Entry Point
// =========================================================================
BOOL CALLBACK MainThreadProc(LPARAM* pParam)
{
	// Initial delay to let UI finish initializing
	Sleep(1000);

	// Log startup
	g_LogProc.LogAddFunc("Connect Server (Version %s) Start.", CONNECT_SERVER_VERSION);

	// Initialize timing
	g_dwJoinServerRefreshTime = GetTickCount();

	// Load configuration files
	ReadServerInfo();      // Server settings from .ini
	ReadServerList();      // Game server list
	ReadConnServerList();  // Connect server peer list

	// Initialize UDP socket for game server communication (port 55557)
	cUdpSoc.CreateSocket();
	cUdpSoc.RecvSet(CONNECTSERVER_UDP_PORT);
	cUdpSoc.Run();

	// Main game server processing loop
	GameServerLoop();

	// Update UI with initial server list
	ShowGameServerListToTreeView();

	// Initialize IOCP for join server (port 44405)
	CreateGIocp(CONNECTSERVER_PORT);

	// Cleanup
	GameServerLoopEnd();

	return 0;
}

// =========================================================================
// ReadServerInfo - Load Configuration from INI File
// =========================================================================
void ReadConnServerList()
{
	;
}

int ReadServerInfo()
{
	static const char* FileName = "Data\\ServerInfo.dat";

	// Check if config file exists
	if (_access(FileName, 0) != 0)
		return 0;

	// Reset client version flags
	ClientV1 = ClientV2 = ClientV3 = 0;
	ClientTEST_V1 = ClientTEST_V2 = ClientTEST_V3 = 0;

	// FTP Server Settings
	GetPrivateProfileString("FtpServerInfo", "FtpServer", "none", FtpServerIpAddress, sizeof(FtpServerIpAddress), FileName);
	FtpServerPort = GetPrivateProfileInt("FtpServerInfo", "Port", 21, FileName);
	GetPrivateProfileString("FtpServerInfo", "Id", "", FtpServerID, sizeof(FtpServerID), FileName);
	GetPrivateProfileString("FtpServerInfo", "Pass", "", FtpServerPass, sizeof(FtpServerPass), FileName);

	// Local Test FTP Settings
	GetPrivateProfileString("FtpServerInfo_LocalTest", "FtpServer", "", FtpServerIpAddress_LocalTest, sizeof(FtpServerIpAddress_LocalTest), FileName);
	FtpServerPort_LocalTest = GetPrivateProfileInt("FtpServerInfo_LocalTest", "Port", 21, FileName);
	GetPrivateProfileString("FtpServerInfo_LocalTest", "Id", "", FtpServerID_LocalTest, sizeof(FtpServerID_LocalTest), FileName);
	GetPrivateProfileString("FtpServerInfo_LocalTest", "Pass", "", FtpServerPass_LocalTest, sizeof(FtpServerPass_LocalTest), FileName);

	// HTTP Server Settings
	GetPrivateProfileStringA("HttpServerInfo", "HttpAddress", "", HTTPServerIpAddress, sizeof(HTTPServerIpAddress), FileName);
	HTTPServerPort = GetPrivateProfileInt("HttpServerInfo", "Port", 80, FileName);
	GetPrivateProfileString("HttpServerInfo", "Id", "", HTTPServerID, sizeof(HTTPServerID), FileName);
	GetPrivateProfileString("HttpServerInfo", "Pass", "", HTTPServerPass, sizeof(HTTPServerPass), FileName);

	// Game Server Info
	GetPrivateProfileString("GameServerInfo", "VersionFileName", "Version.wvd", FtpServerFileName, sizeof(FtpServerFileName), FileName);
	GetPrivateProfileString("GameServerInfo", "VersionFileName", "Version.wvd", HTTPServerFileName, sizeof(HTTPServerFileName), FileName);
	GetPrivateProfileString("GameServerInfo", "ClientVersion", "00.00.00", ClientVersion, sizeof(ClientVersion), FileName);

	g_LogProc.LogAddFunc("Client Version : %s", ClientVersion);

	// Test client version (default to 0.0.0)
	strcpy(ClientVersion_TEST, "00.00.00");

	// Local Connect Only setting
	g_bLocalNetworkConnectOnly = GetPrivateProfileInt("LocalNetworkConnect", "LocalConnectOnly", 0, FileName);

	// Update toolbar button state
	if (hToolBar)
	{
		//SendMessageA(hToolBar, TB_CHECKBUTTON, ID_TOOL_LOCAL_ONLY,  MAKELPARAM(g_bLocalNetworkConnectOnly, 0));

		if (g_bLocalNetworkConnectOnly)
			SendMessageA(hToolBar, TB_CHANGEBITMAP, 0x1F44, 8);   // Checked state
		else
			SendMessageA(hToolBar, TB_CHANGEBITMAP, 0x1F44, 3);   // Unchecked state
	}

	// Download type (0 = FTP, 1 = HTTP)
	g_iDownLoadType = GetPrivateProfileInt("GameServerInfo", "DownloadType", 0, FileName);

	// Parse client version string (e.g., "1.04.05" → V1=1, V2=4, V3=5)
	char szTemp[32];
	strncpy(szTemp, ClientVersion, sizeof(szTemp));

	char* token = strtok(szTemp, ".");
	ClientV1 = token ? atoi(token) : 0;

	token = strtok(NULL, ".");
	ClientV2 = token ? atoi(token) : 0;

	token = strtok(NULL, ".");
	ClientV3 = token ? atoi(token) : 0;

	// Parse test version similarly
	strncpy(szTemp, ClientVersion_TEST, sizeof(szTemp));
	token = strtok(szTemp, ".");
	ClientTEST_V1 = token ? atoi(token) : 0;
	token = strtok(NULL, ".");
	ClientTEST_V2 = token ? atoi(token) : 0;
	token = strtok(NULL, ".");
	ClientTEST_V3 = token ? atoi(token) : 0;

	g_LogProc.LogAddFunc("Patch Server Info Loaded");

	// Generate server info file for clients
	CFileServerInfoMake();

	// DDoS Protection Settings
	DWORD dwPktPerSec = GetPrivateProfileInt("DDoS", "PacketPerSec", 0, FileName);
	if (dwPktPerSec)
		g_dwMaxPacketPerSec = dwPktPerSec;

	DWORD dwInvalidPkt = GetPrivateProfileInt("DDoS", "InvalidPacketLimit", 0, FileName);
	if (dwInvalidPkt)
		g_dwMaxInvalidPackets = dwInvalidPkt;

	DWORD dwSecBan = GetPrivateProfileInt("DDoS", "ProhibitSeconds", 0, FileName);
	if (dwSecBan)
		g_dwBanDurationMs = dwSecBan * 1000;

	// Whitelisted IPs for DDoS checking
	GetPrivateProfileString("DDoS", "NoCheckingIP", "218.234.76.254", DDOSNoCheckingIpAddress, sizeof(DDOSNoCheckingIpAddress), FileName);

	// Flag for multicast list rebuild
	g_bMULTICAST_LIST_REMAKE = TRUE;

	return 1;
}

// =========================================================================
// ReadServerList - Load Server List from File
// =========================================================================
void ReadServerList()
{
	// Load file into memory buffer
	FILE* pFile = fopen("data\\ServerList.dat", "rb");  // Binary mode for exact size
	if (!pFile)
	{
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "ServerList.dat File Read Error.");
		return;
	}

	// Get file size
	fseek(pFile, 0, SEEK_END);
	long fileSize = ftell(pFile);
	fseek(pFile, 0, SEEK_SET);

	if (fileSize <= 0 || fileSize > 1024 * 1024)  // Max 1MB config file
	{
		fclose(pFile);
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "ServerList.dat invalid size");
		return;
	}

	// Allocate buffer and read file
	char* pBuffer = new char[fileSize + 1];  // +1 for null terminator
	if (!pBuffer)
	{
		fclose(pFile);
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "Memory allocation failed");
		return;
	}

	size_t bytesRead = fread(pBuffer, 1, fileSize, pFile);
	pBuffer[bytesRead] = '\0';  // Null-terminate for safety
	fclose(pFile);

	// Initialize server list slots
	GSListCount = 0;
	EnterCriticalSection(&gameservercriti);

	for (int n = 0; n < MAX_GAMESERVERLIST; ++n)
	{
		GameServerList[n].ServerCode = -1;
		GameServerList[n].Live = 0;
		GameServerList[n].LiveTime = GetTickCount();
		GameServerList[n].UserCount = n;  // TEST field
		GameServerList[n].Percent = 0;
		GameServerList[n].bShow = FALSE;
		ZeroMemory(GameServerList[n].Name, sizeof(GameServerList[n].Name));
		ZeroMemory(GameServerList[n].IpAddress, sizeof(GameServerList[n].IpAddress));
	}

	// Parse using CWzMemScript
	CWzMemScript script;
	script.SetBuffer(pBuffer, static_cast<int>(bytesRead));

	SMDToken token;
	while ((token = script.GetToken()) != END)
	{
		// First token: server number (as number)
		if (token != NUMBER)
			continue;  // Skip invalid entries

		int snum = script.GetNumber() & 0xFF;  // Cast to byte as original

		// Validate server number
		if (snum < 0 || snum >= MAX_GAMESERVERLIST)
			continue;

		// Set server code
		GameServerList[snum].ServerCode = static_cast<BYTE>(script.GetNumber());

		// Next token: server name (string)
		token = script.GetToken();
		if (token == NAME)
		{
			strncpy_s(GameServerList[snum].Name, sizeof(GameServerList[snum].Name), script.GetString(), _TRUNCATE);
		}

		// Next token: IP address (string)
		token = script.GetToken();
		if (token == NAME)
		{
			strncpy_s(GameServerList[snum].IpAddress, sizeof(GameServerList[snum].IpAddress), script.GetString(), _TRUNCATE);
		}

		// Next token: port number
		token = script.GetToken();
		if (token == NUMBER)
		{
			GameServerList[snum].Port = script.GetNumber();
		}

		// Initialize load percentage
		GameServerList[snum].Percent = 0;

		// Next token: SHOW/HIDE flag
		token = script.GetToken();
		if (token == NAME)
		{
			// Case-sensitive comparison (matches original)
			if (strcmp(script.GetString(), "SHOW") == 0)
			{
				GameServerList[snum].bShow = TRUE;
			}
			else
			{
				GameServerList[snum].bShow = FALSE;
			}
		}
		else
		{
			// Default to hidden
			GameServerList[snum].bShow = FALSE;
		}

		// TEST build: force live status
#ifdef TEST_MD
		GameServerList[snum].Live = TRUE;
#endif

		// Increment valid server count
		++GSListCount;
	}

	LeaveCriticalSection(&gameservercriti);

	// Cleanup
	delete[] pBuffer;
	// Log success
	g_LogProc.LogAddFunc("Game Server List Update OK.");

	// Rebuild internal structures
	CServerListMake();
}

// =========================================================================
// GameServerInfoUpdate - Update Server Info from Heartbeat
// =========================================================================
//GameServerInfoUpdate(lpMsg->ServerCode, lpMsg->Percent, lpMsg->UserCount, 0, 0, lpMsg->MaxUserCount);
void GameServerInfoUpdate(int servercode, BYTE percent, WORD maxuser, WORD usercount, WORD freeusers, WORD serverflag)
{
	EnterCriticalSection(&gameservercriti);

	for (int n = 0; n < MAX_GAMESERVERLIST; n++)
	{
		if (GameServerList[n].ServerCode == servercode)
		{
			GameServerList[n].Percent = percent;
			GameServerList[n].LiveTime = GetTickCount();

			GameServerList[n].MaxUserCount = maxuser;
			GameServerList[n].PCbangCount = maxuser - freeusers;
			GameServerList[n].UserCount = freeusers;

			GameServerList[n].ServerFlag = serverflag;

			if (GameServerList[n].Live == 0)
			{
				g_LogProc.LogAddFunc(
					" %d(%s) Server update complete.  %s(%d)",
					servercode,
					GameServerList[n].Name,
					GameServerList[n].IpAddress,
					GameServerList[n].Port);
			}

			GameServerList[n].Live = TRUE;

			LeaveCriticalSection(&gameservercriti);

			g_bGameServerLive = TRUE;
			return;
		}
	}

	LeaveCriticalSection(&gameservercriti);
}

// =========================================================================
// GameServerInfoTimeCheck - Remove Timed-Out Servers
// =========================================================================
void GameServerInfoTimeCheck()
{
	EnterCriticalSection(&gameservercriti);

	for (int n = 0; n < MAX_GAMESERVERLIST; n++)
	{
		// Local server is always alive
		if (!strcmp(GameServerList[n].ServerVersion, "LOCAL"))
		{
			GameServerList[n].Live = 1;
		}
		else
		{
			if (GameServerList[n].Live && (GetTickCount() - GameServerList[n].LiveTime) > 10000)
			{
				GameServerList[n].Live = 0;
				g_LogProc.LogAddFunc("%s TIME OUT Server Delete %d.", GameServerList[n].Name, GetTickCount() - GameServerList[n].LiveTime);
				GameServerList[n].ServerFlag = 0;
				GameServerList[n].ServerType = 0;
				memset(GameServerList[n].ServerVersion, 0, sizeof(GameServerList[n].ServerVersion));
				memset(GameServerList[n].ClientVersion, 0, sizeof(GameServerList[n].ClientVersion));
				g_bGameServerLive = FALSE;
			}
		}
	}

	LeaveCriticalSection(&gameservercriti);

	CServerListMake();
	CServerUserCountListMake();

	if (g_bJoinServerLive)
	{
		if ((GetTickCount() - g_dwJoinServerRefreshTime) > 5000)
		{
			g_bJoinServerLive = FALSE;
			g_JoinServerRecvQCount = 101;
			g_bJoinServerConnectLimit = 0;
			g_LogProc.LogAddFuncColor(2, "TIME OUT JoinServer");
		}
	}
}

// =========================================================================
// CheckReloadServerListKey - Hotkey to Reload Server List
// =========================================================================
void CheckReloadServerListKey()
{
	// VK_RETURN = 13
	if (GetAsyncKeyState(VK_RETURN) & 0x8000)
	{
		g_LogProc.LogAddFunc("ReLoad ServerList File.");
		ReadServerList();
	}
}

// =========================================================================
// Background Thread for Server Monitoring
// =========================================================================
DWORD CALLBACK LoopThread(LPVOID lpParam)
{
	GetTickCount();  // Original: dummy call, possibly for timing init

	while (!g_bDoTerminateLoopThread)
	{
		GameServerInfoTimeCheck();
		Sleep(1000);  // 0x3E8 = 1000ms
	}

	return 0;
}


void GameServerLoop()
{
	g_bDoTerminateLoopThread = FALSE;
	DWORD ThreadID;
	if ((LoopThreadHandle = CreateThread(NULL, 0, LoopThread, 0, 0, &ThreadID)) == NULL)
	{
		return;
	}
}

void GameServerLoopEnd()
{
	if (LoopThreadHandle)
	{
		CloseHandle(LoopThreadHandle);
		LoopThreadHandle = NULL;
	}
}

// =========================================================================
// ShowGameServerListToTreeView - Populate UI Tree with Servers
// =========================================================================
void ShowGameServerListToTreeView()
{
	// Clear TreeView
	SendMessage(hWnd, TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT);

	int ServerGroupCount[128];
	int ServerGroupCreated[128];

	memset(ServerGroupCount, 0, sizeof(ServerGroupCount));
	memset(ServerGroupCreated, 0, sizeof(ServerGroupCreated));

	//dword_4B92F8 = 0;

	EnterCriticalSection(&gameservercriti);

	//
	// Count visible/live servers per group
	//
	for (int n = 0; n < MAX_GAMESERVERLIST; n++)
	{
		GameServerList_Struct* lpServer = &GameServerList[n];

		if (lpServer->ServerCode > -1 && lpServer->Live)
		{
			ServerGroupCount[n / 20]++;
		}
	}

	//
	// Insert tree nodes
	//
	for (int n = 0; n < MAX_GAMESERVERLIST; n++)
	{
		GameServerList_Struct* lpServer = &GameServerList[n];

		if (lpServer->ServerCode <= -1 || !lpServer->Live)
		{
			continue;
		}

		int Group = n / 20;

		//
		// Create root group node once
		//
		if (!ServerGroupCreated[Group])
		{
			ServerGroupCreated[Group] = 1;
			char szGroupName[256];
			wsprintf(szGroupName, "Game Server - %d (%d)", Group + 1, ServerGroupCount[Group]);
			CTreeCtl.InsertServerNode(0, 0, szGroupName);
		}

		//
		// Extended info mode
		//
		if (lpServer->ServerType == 1)
		{
			char szText[256];
			const char* szPKType;

			if (lpServer->ServerFlag)
			{
				szPKType = "PK";
			}
			else
			{
				szPKType = "NON-PK";
			}

			//
			// Visible server
			//
			if (lpServer->bShow)
			{
				wsprintf(szText, "%s : [%s] SV:%s, CV:%s", lpServer->Name, szPKType, lpServer->ServerVersion, lpServer->ClientVersion);
				strcpy(lpServer->DisplayName, szText);
				CTreeCtl.InsertSubServerNode(1, 1, szText);
			}
			//
			// Hidden server
			//
			else
			{
				wsprintf(szText, "%s : [%s] GV:%s, CL:%s", lpServer->Name, szPKType, lpServer->ServerVersion, lpServer->ClientVersion);
				strcpy(lpServer->DisplayName, szText);
				CTreeCtl.InsertSubServerNode(2, 2, szText);
			}
		}
		//
		// Legacy/simple mode
		//
		else
		{
			strcpy(lpServer->DisplayName, lpServer->Name);

			if (lpServer->bShow)
			{
				CTreeCtl.InsertSubServerNode(1, 1, lpServer->Name);
			}
			else
			{
				CTreeCtl.InsertSubServerNode(2, 2, lpServer->Name);
			}
		}
	}

	LeaveCriticalSection(&gameservercriti);
}

// =========================================================================
// InverseGameServer_ShowHide - Toggle Server Visibility in UI
// =========================================================================
void InverseGameServer_ShowHide(HWND hWnd, HTREEITEM* hItem, char* ServerName)
{
	EnterCriticalSection(&gameservercriti);

	for (int n = 0; n < MAX_GAMESERVERLIST; n++)
	{
		if (GameServerList[n].ServerCode > -1 && GameServerList[n].Live && strcmp(GameServerList[n].Name, ServerName) == 0)
		{
			char Text[128];
			const char* CurrentState = GameServerList[n].bShow ? "SHOW" : "HIDE";
			const char* NextState = GameServerList[n].bShow ? "HIDE" : "SHOW";

			wsprintf(Text, "%s  Current State : %s\n\nWill you change this State to %s ?",
				GameServerList[n].Name, CurrentState, NextState);

			if (MessageBox(0, Text, "Ask GameServer State Change", MB_YESNO | MB_ICONQUESTION | MB_SYSTEMMODAL) == IDYES)
			{
				GameServerList[n].bShow = !GameServerList[n].bShow;

				TVITEM tvi;

				tvi.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
				tvi.hItem = *hItem;

				if (GameServerList[n].bShow)
				{
					tvi.iImage = 1;
					tvi.iSelectedImage = 1;
				}
				else
				{
					tvi.iImage = 2;
					tvi.iSelectedImage = 2;
				}
				SendMessage(hWnd, TVM_SETITEM, 0, (LPARAM)&tvi);
			}
			break;
		}
	}
	LeaveCriticalSection(&gameservercriti);
}

// =========================================================================
// CheckLocalNetworkConnectOnly - Handle Local-Only Connection Mode
// =========================================================================

void CheckLocalNetworkConnectOnly()
{
	if (g_bLocalConnectConfirm != g_bLocalNetworkConnectOnly)
	{
		// Switching FROM normal mode TO local-only mode
		if (!g_bLocalConnectConfirm)
		{
			EnterCriticalSection(&g_UserLock);

			OBJECTSTRUCT* lpObj = gObj;  // Start of object array
			int n = 0;

			// Loop until reaching g_UserLock (end marker after object array)
			while ((int)lpObj < (int)&g_UserLock.RecursionCount)
			{
				if (lpObj->Connected)
				{
					// Invalid socket
					if (lpObj->m_socket == INVALID_SOCKET)
					{
						lpObj->Connected = FALSE;
						g_LogProc.LogAddFuncColor(2, "CONNECT:ON SOCKET:INVALID -> INDEX:%d", n);
					}
					else
					{
						// Force close with SO_DONTLINGER
						BOOL bDontLinger = TRUE;
						setsockopt(lpObj->m_socket, SOL_SOCKET, SO_DONTLINGER,
							(char*)&bDontLinger, sizeof(bDontLinger));
						closesocket(lpObj->m_socket);

						lpObj->Connected = FALSE;
						lpObj->m_socket = INVALID_SOCKET;

						// Add index to free queue (std::deque<int> g_ObjIndexQ)
						g_ObjIndexQ.push(n);

						InterlockedDecrement(&g_iSockConnNum);
					}
				}

				lpObj++;  // Advances by sizeof(OBJECTSTRUCT) = 6187 bytes
				++n;
			}

			LeaveCriticalSection(&g_UserLock);
		}

		g_bLocalConnectConfirm = g_bLocalNetworkConnectOnly;
	}
}

// =========================================================================
// MC_SEND_ConnectServerInfo - Broadcast Server Status via Multicast
// =========================================================================
void MC_SEND_ConnectServerInfo()
{
	if (!g_bDoMultiCast || !g_bMultiCastOn)
	{
		return;
	}

	int CpuUsage = -999;
	int MemoryUsage = -999;

	PMSG_CONNECTSERVER_INFO pMsg;

	pMsg.h.c = PMHC_BYTE;
	pMsg.h.size = sizeof(PMSG_CONNECTSERVER_INFO);
	pMsg.h.headcode = 0x01;

	pMsg.MultiCastIP = inet_addr(g_szMC_IPADDR);
	pMsg.PersonalShopIP = inet_addr(g_szPB_IPADDR);

	//
	// Query performance monitor
	//
	if (g_PerfMonEx.CollectQueryData())
	{
		CpuUsage = g_PerfMonEx.GetCounterValue(g_iPERF_IDX_CPU);

		MemoryUsage = g_PerfMonEx.GetCounterValue(g_iPERF_IDX_MEM);

		if (CpuUsage == -999)
		{
			g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "Failed to query PDH CPU counter");
		}

		if (MemoryUsage == -999)
		{
			g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "Failed to query PDH memory counter");
		}
	}
	else
	{
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "Failed to query PDH data");
	}

	//
	// Fill statistics
	//
	pMsg.ConnectionCount = g_iSockConnNum;
	pMsg.CpuUsage = CpuUsage;
	pMsg.MemoryUsage = MemoryUsage;

	//
	// Send multicast packet
	//
	g_CsInfoMultiCast.SendData((char*)&pMsg, sizeof(PMSG_CONNECTSERVER_INFO));
}

// =========================================================================
// MC_RECV_ConnectServerInfo - Process Incoming Multicast Packets
// =========================================================================
std::map<int, CSINFO_NODE> g_mapCSINFO_LIST;
void MC_RECV_ConnectServerInfo(PMSG_CONNECTSERVER_INFO* lpMsg)
{
	if (!lpMsg)
	{
		g_LogProc.LogAddFuncColor(2, "MC_RECV_ConnectServerInfo() lpMsg == NULL");
		return;
	}

	if (!inet_ntoa(*(in_addr*)&lpMsg->MultiCastIP))
	{
		g_LogProc.LogAddFuncColor(2, "MC_RECV_ConnectServerInfo() inet_ntoa failed");
	}

	EnterCriticalSection(&g_critCSINFO_LIST);

	std::map<int, CSINFO_NODE>::iterator it = g_mapCSINFO_LIST.find((int)lpMsg->MultiCastIP);

	if (it == g_mapCSINFO_LIST.end())
	{
		CSINFO_NODE Info;

		Info.MultiCastIP = lpMsg->MultiCastIP;
		Info.PersonalShopIP = lpMsg->PersonalShopIP;
		Info.ConnectionCount = lpMsg->ConnectionCount;
		Info.CpuUsage = lpMsg->CpuUsage;
		Info.MemoryUsage = lpMsg->MemoryUsage;
		Info.dwTickCount = GetTickCount();

		g_mapCSINFO_LIST.insert(std::make_pair((int)lpMsg->MultiCastIP, Info));
	}
	else
	{
		CSINFO_NODE& Info = it->second;
		Info.MultiCastIP = lpMsg->MultiCastIP;
		Info.PersonalShopIP = lpMsg->PersonalShopIP;
		Info.ConnectionCount = lpMsg->ConnectionCount;
		Info.CpuUsage = lpMsg->CpuUsage;
		Info.MemoryUsage = lpMsg->MemoryUsage;
		Info.dwTickCount = GetTickCount();
	}

	LeaveCriticalSection(&g_critCSINFO_LIST);
}

// =========================================================================
// MC_ShowConnectServerList - Display Multicast Server List in UI
// =========================================================================

void MC_ShowConnectServerList()
{
	EnterCriticalSection(&g_critCSINFO_LIST);

	std::map<int, CSINFO_NODE>::iterator it = g_mapCSINFO_LIST.begin();

	while (it != g_mapCSINFO_LIST.end())
	{
		CSINFO_NODE& Info = it->second;

		// Remove expired entries (5 seconds)
		if ((GetTickCount() - Info.dwTickCount) >= 5000)
		{
			std::map<int, CSINFO_NODE>::iterator EraseIt = it++;
			g_mapCSINFO_LIST.erase(EraseIt);
		}
		else
		{
			IpSelectDlg_InsertListViewCtrl(
				*(in_addr*)&Info.MultiCastIP, *(in_addr*)&Info.PersonalShopIP,
				Info.ConnectionCount, Info.CpuUsage, Info.MemoryUsage);

			++it;
		}
	}

	LeaveCriticalSection(&g_critCSINFO_LIST);
}

// =========================================================================
// MC_SEND_CcsShareInfo - Broadcast Server Status to Other Connect Servers
// =========================================================================
void MC_SEND_CcsShareInfo()
{
	if (!g_bDoMultiCast || !g_bMultiCastOn)
	{
		return;
	}

	EnterCriticalSection(&gameservercriti);

	if (g_cCCS_INFOSHARE_BUF.ShareCount > 0)
	{
		if (g_bGameServerLive)
		{
		SEND_PACKET:

			if (!g_bJoinServerLive)
			{
				g_cCCS_INFOSHARE_BUF.JoinServerCount = 0;
			}

			WORD size = MAKEWORD(g_cCCS_INFOSHARE_BUF.sizeL,g_cCCS_INFOSHARE_BUF.sizeH);
			g_CsInfoMultiCast.SendData((char*)&g_cCCS_INFOSHARE_BUF,size);

			goto EXIT_LABEL;
		}

		if (g_bJoinServerLive)
		{
			g_cCCS_INFOSHARE_BUF.ShareCount = 0;
			g_cCCS_INFOSHARE_BUF.c			= PMHC_WORD;
			g_cCCS_INFOSHARE_BUF.sizeH		= 0;
			g_cCCS_INFOSHARE_BUF.sizeL		= 0x10;
			g_cCCS_INFOSHARE_BUF.head		= 0x03;
			goto SEND_PACKET;
		}
	}

EXIT_LABEL:

	LeaveCriticalSection(&gameservercriti);
}

// =========================================================================
// MC_RECV_CcsShareInfo - Process Incoming CCS Share Packets
// =========================================================================
void MC_RECV_CcsShareInfo(PMSG_CCS_INFOSHARE* lpMsg)
{
	if (!lpMsg)
	{
		g_LogProc.LogAddFuncColor(2, "MC_RECV_CcsShareInfo ERROR : lpcBUF == NULL");
		return;
	}

	if (lpMsg->ShareCount > 1000)
	{
		g_LogProc.LogAddFuncColor(2, "MC_RECV_CcsShareInfo ERROR : pMsg->iGameServerInfoCount Out of Bound");
		return;
	}

	EnterCriticalSection(&gameservercriti);

	g_JoinServerRecvQCount = lpMsg->JoinServerCount;

	if (!g_bJoinServerLive)
	{
		g_LogProc.LogAddFunc("JoinServer udpate complete.");
		g_bJoinServerLive = TRUE;
	}

	g_bJoinServerConnectLimit = FALSE;
	g_dwJoinServerRefreshTime = GetTickCount();

	if (lpMsg->ShareCount <= 0)
	{
		LeaveCriticalSection(&gameservercriti);
		return;
	}

	for (int n = 0; n < lpMsg->ShareCount; n++)
	{
		PMSG_CCS_INFOLIST* lpInfo = &lpMsg->Entry[n];

		for (int i = 0; i < MAX_CONNECTOBJECT; i++)
		{
			GameServerList_Struct* lpServer = &GameServerList[i];

			if (lpServer->ServerCode != lpInfo->ServerCode)
			{
				continue;
			}

			lpServer->ServerType = lpInfo->ServerType;
			lpServer->LiveTime = GetTickCount();
			lpServer->MaxUserCount =lpInfo->MaxUserCount;
			lpServer->ServerFlag =lpInfo->ServerFlag;
			lpServer->PCbangCount =lpInfo->PCbangCount;
			lpServer->UserCount =lpInfo->UserCount;

			if (!lpServer->Live)
			{
				g_LogProc.LogAddFunc(" %d(%s) Server update complete. %s(%d)",
					lpServer->ServerCode,lpServer->Name,lpServer->IpAddress,lpServer->Port);

				lpServer->State = 0;
				lpServer->bShow = 0;
				memset(lpServer->ServerVersion, 0, 32);
				memset(lpServer->ClientVersion, 0, 32);
			}
			lpServer->Live = TRUE;

			break;
		}
	}

	LeaveCriticalSection(&gameservercriti);
}

// =========================================================================
// MULTICAST_RECVNOTIFY_PROC - Global Receive Callback
// Dispatches multicast packets to appropriate handlers
// =========================================================================
void MULTICAST_RECVNOTIFY_PROC(CWzMultiCastSock* lpMultiCast, int IpAddress, int Protocol, int SubCode, char* lpMsg, int Size)
{
	g_LogProc.LogAddFunc("MULTICAST_RECVNOTIFY_PROC Ok!");
	switch (Protocol)
	{
	case 0x01:
	{
		MC_RECV_ConnectServerInfo((PMSG_CONNECTSERVER_INFO*)lpMsg);
	}
	break;

	case 0x03:
	{
		MC_RECV_CcsShareInfo((PMSG_CCS_INFOSHARE*)lpMsg);
	}
	break;
	}
}

// =========================================================================
// MULTICAST_ERRORNOTIFY_PROC - Global Error Callback
// Logs multicast errors and stops reception
// =========================================================================
void  MULTICAST_ERRORNOTIFY_PROC(CWzMultiCastSock* lpMultiCast, int ErrorCode, char* szError)
{
	g_LogProc.LogAddFuncColor(2, "MultiCasting Error Occured : %d, %s", ErrorCode, szError);
	lpMultiCast->EndMultiCast();
	g_bMultiCastOn = FALSE;
	SendMessageA(hToolBar, TB_CHECKBUTTON, 0x1F46, MAKELONG(FALSE, 0));
}