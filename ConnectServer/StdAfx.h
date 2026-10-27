#pragma once

#include "targetver.h"
#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files
#include <windows.h>
// C RunTime Header Files
#include <stdlib.h>
#include <malloc.h>
#include <memory.h>
#include <tchar.h>
#include <time.h>
#include <io.h>
#include <process.h>
#include <queue>
#include <map>
#include <vector>
#include <deque>

#include <WinSock2.h>
#include <ws2tcpip.h>
//#include <CommCtrl.h>

#include <Pdh.h>

#define _CRT_SECURE_NO_DEPRECATE
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "Msimg32.lib")

#pragma comment(lib, "pdh.lib")

#include "..//include//ProDef.h"

#include "resource.h"
#include "LogProc.h"
#include "LogToFile.h"
#include "LogToListBox.h"

#define CONNECT_SERVER_VERSION "20060123"
//#define __DEBUG_LOG

void WzLog(char* msg, ...);
void WzLog2(char* msg, ...);
void WzLog3(char* msg, ...);
void WzLog4(char* msg, ...);
void WzLog5(char* msg, ...);

extern CLogProc g_LogProc;
extern CLogToFile g_LogGSInfo;
extern CLogToFile g_LogFileError;