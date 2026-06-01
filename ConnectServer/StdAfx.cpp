#include "StdAfx.h"
#include "LogProc.h"
#include "LogToListBox.h"

CCriticalSec WZLogCS;
CCriticalSec WZGSLogCS;
CLogToFile g_LogGSInfo("CS_LOG_GSINFO", ".\\LOG_GSINFO");
CLogProc g_LogProc(40, 30, NULL);
CLogToFile g_LogFileError("CS_LOG_ERROR", ".\\LOG_ERROR");
CLogToListBox g_LogListBoxError;