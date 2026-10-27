#include "StdAfx.h"
#include "LogProc.h"
#include "LogToListBox.h"
#include "CriticalSec.h"

CCriticalSec WZLogCS;
CCriticalSec WZGSLogCS;
CLogToFile g_LogGSInfo("CS_LOG_GSINFO", ".\\LOG_GSINFO");
CLogProc g_LogProc(40, 30, NULL);
CLogToFile g_LogFileError("CS_LOG_ERROR", ".\\LOG_ERROR");
CLogToListBox g_LogListBoxError;

void WzLog(char* msg, ...)
{
	return;
	CCSLock	cs(&WZLogCS);

	FILE *	logfp=NULL;

	int LogMYear, LogMonth, LogMDay;
	char		szTemp[50];
	struct tm * today;
	time_t		ltime;
	
	time( &ltime );
	today = localtime( &ltime );
	
	today->tm_year += 1900;

	LogMYear = today->tm_year;
	LogMonth = today->tm_mon+1;
	LogMDay  = today->tm_mday;

	wsprintf(szTemp, "log\\Errorlog_%02d%02d%02d.log",LogMYear, LogMonth,LogMDay);

	if( logfp!=NULL ) {
		//fclose(logfp);		
	}
	logfp = fopen(szTemp, "a+t");
	if( logfp == NULL )
	{
		printf("로그 파일 생성 에러\n");
	}

	char szBuffer[512]="";
	va_list		pArguments;

	va_start(pArguments, msg);
    vsprintf(szBuffer, msg, pArguments);
    va_end(pArguments);

	fprintf(logfp, "%s\n",szBuffer);
	fclose(logfp);
}


void WzLog2(char* msg, ...)
{
	return;
	CCSLock	cs(&WZLogCS);

	FILE *	logfp=NULL;

	int LogMYear, LogMonth, LogMDay;
	char		szTemp[50];
	struct tm * today;
	time_t		ltime;
	
	time( &ltime );
	today = localtime( &ltime );
	
	today->tm_year += 1900;

	LogMYear = today->tm_year;
	LogMonth = today->tm_mon+1;
	LogMDay  = today->tm_mday;

	wsprintf(szTemp, "log\\Errlog_%02d%02d%02d.log",LogMYear, LogMonth,LogMDay);

	if( logfp!=NULL ) {
		//fclose(logfp);		
	}
	logfp = fopen(szTemp, "a+t");
	if( logfp == NULL )
	{
		printf("로그 파일 생성 에러\n");
	}

	char szBuffer[512]="";
	va_list		pArguments;

	va_start(pArguments, msg);
    vsprintf(szBuffer, msg, pArguments);
    va_end(pArguments);

	fprintf(logfp, "%s\n",szBuffer);
	fclose(logfp);
}


void WzLog3(char* msg, ...)
{	
	CCSLock	cs(&WZLogCS);

	FILE *	logfp=NULL;

	int LogMYear, LogMonth, LogMDay;
	char		szTemp[50];
	struct tm * today;
	time_t		ltime;
	
	time( &ltime );
	today = localtime( &ltime );
	
	today->tm_year += 1900;

	LogMYear = today->tm_year;
	LogMonth = today->tm_mon+1;
	LogMDay  = today->tm_mday;

	wsprintf(szTemp, "log\\log_%02d%02d%02d.log",LogMYear, LogMonth,LogMDay);

	if( logfp!=NULL ) {
		//fclose(logfp);		
	}
	logfp = fopen(szTemp, "a+t");
	if( logfp == NULL )
	{
		printf("로그 파일 생성 에러\n");
		return;
	}

	char szBuffer[512]="";
	va_list		pArguments;

	va_start(pArguments, msg);
    vsprintf(szBuffer, msg, pArguments);
    va_end(pArguments);

	fprintf(logfp, "%s\n",szBuffer);
	fclose(logfp);
}

void WzLog4(char* msg, ...)
{	
	CCSLock	cs(&WZGSLogCS);

	FILE *	logfp=NULL;

	int LogMYear, LogMonth, LogMDay;
	char		szTemp[50];
	struct tm * today;
	time_t		ltime;
	
	time( &ltime );
	today = localtime( &ltime );
	
	today->tm_year += 1900;

	LogMYear = today->tm_year;
	LogMonth = today->tm_mon+1;
	LogMDay  = today->tm_mday;

	wsprintf(szTemp, "log\\GSlog_%02d%02d%02d.log",LogMYear, LogMonth,LogMDay);

	if( logfp!=NULL ) {
		//fclose(logfp);		
	}
	logfp = fopen(szTemp, "a+t");
	if( logfp == NULL )
	{
		printf("로그 파일 생성 에러\n");
		return;
	}

	char szBuffer[512]="";
	va_list		pArguments;

	va_start(pArguments, msg);
    vsprintf(szBuffer, msg, pArguments);
    va_end(pArguments);

	fprintf(logfp, "%s\n",szBuffer);
	fclose(logfp);
}


void WzLog5(char* msg, ...)
{
	FILE *	logfp=NULL;

	int LogMYear, LogMonth, LogMDay;
	char		szTemp[50];
	struct tm * today;
	time_t		ltime;
	
	time( &ltime );
	today = localtime( &ltime );
	
	today->tm_year += 1900;

	LogMYear = today->tm_year;
	LogMonth = today->tm_mon+1;
	LogMDay  = today->tm_mday;

	wsprintf(szTemp, "log\\CS_PACKET_%02d%02d%02d.log",LogMYear, LogMonth,LogMDay);

	if( logfp!=NULL ) {
		//fclose(logfp);		
	}
	logfp = fopen(szTemp, "a+t");
	if( logfp == NULL )
	{
		printf("로그 파일 생성 에러\n");
		return;
	}

	char szBuffer[512]="";
	va_list		pArguments;

	va_start(pArguments, msg);
    vsprintf(szBuffer, msg, pArguments);
    va_end(pArguments);

	fprintf(logfp, "%s\n",szBuffer);
	fclose(logfp);
}