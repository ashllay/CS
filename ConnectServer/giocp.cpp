#include "StdAfx.h"
#include "giocp.h"

#include "CS.h"
#include "IpAddrMng.h"
#include "Protocol.h"
#include "User.h"


// =========================================================================
// Global Variables
// =========================================================================
SOCKET				g_Listen = INVALID_SOCKET;
HANDLE				g_IocpThreadHandle;
CRITICAL_SECTION	g_critERROR_LOG = { NULL, 0, 0, NULL, NULL, 0 };
CPIM				g_cPIM(64);
int					g_PIMQueueIndex = 0;
CRITICAL_SECTION	g_CS = { NULL, 0, 0, NULL, NULL, 0 };
int					g_ServerPort = 0;
HANDLE				g_ThreadHandles[MAX_WORKER_THREAD];
char				ErrorMsg[1024];
DWORD				g_dwThreadCount = 0;
HANDLE				g_CompletionPort = NULL;
LONG				g_iSockConnNum = 0;

// =========================================================================
// GetLastErrorMsg - Format Winsock Error Message
// =========================================================================
void GetLastErrorMsg()
{
	LPVOID lpMsgBuf;
	EnterCriticalSection(&g_critERROR_LOG);
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		GetLastError(),
		0, // Default language
		(LPTSTR)&lpMsgBuf,
		0,
		NULL
	);

	strcpy(ErrorMsg, (LPCTSTR)lpMsgBuf);
	// Free the buffer.
	LocalFree(lpMsgBuf);
	LeaveCriticalSection(&g_critERROR_LOG);
}

// =========================================================================
// CreateGIocp - Initialize IOCP Server
// =========================================================================
int CreateGIocp(int server_port)
{
	gObjInit();              // Initialize connection pool
	g_ServerPort = server_port;
	return IocpServerWorker();
}

// =========================================================================
// DestroyGIocp - Cleanup IOCP Server
// =========================================================================
void DestroyGIocp()
{
	// Terminate worker threads
	for (DWORD dwCPU = 0; dwCPU < g_dwThreadCount; dwCPU++)
	{
		if (g_ThreadHandles[dwCPU])
		{
			TerminateThread(g_ThreadHandles[dwCPU], 0);
			CloseHandle(g_ThreadHandles[dwCPU]);
			g_ThreadHandles[dwCPU] = NULL;
		}
	}

	// Terminate main IOCP thread
	if (g_IocpThreadHandle)
	{
		TerminateThread(g_IocpThreadHandle, 0);
		CloseHandle(g_IocpThreadHandle);
		g_IocpThreadHandle = NULL;
	}

	// Close completion port
	if (g_CompletionPort)
	{
		CloseHandle(g_CompletionPort);
		g_CompletionPort = NULL;
	}

	// Cleanup connection pool
	gObjEnd();
}

// =========================================================================
// CreateListenSocket - Create and Bind Listening Socket
// =========================================================================
int CreateListenSocket()
{
	sockaddr_in addr;

	// Create overlapped socket
	g_Listen = WSASocketA(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (g_Listen == INVALID_SOCKET)
	{
		EnterCriticalSection(&g_CS);
		//g_CS.Lock();
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "WSASocket() failed with error %d", WSAGetLastError());
		LeaveCriticalSection(&g_CS);
		//g_CS.UnLock();
		return FALSE;
	}

	// Bind to port
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons(static_cast<u_short>(g_ServerPort));

	if (bind(g_Listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
	{
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "bind error (server may be running already)");
		return FALSE;
	}

	// Start listening
	if (listen(g_Listen, SOMAXCONN) == SOCKET_ERROR)
	{
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "listen() failed with error %d", WSAGetLastError());
		return FALSE;
	}
	g_LogProc.LogAddFunc("Server Listen OK. Port: %d", g_ServerPort);
	return TRUE;
}

// =========================================================================
// IocpServerWorker - Main IOCP Server Loop
// =========================================================================
//#define __TEST
#ifdef __TEST
DWORD WINAPI IocpServerWorker()
{
	SYSTEM_INFO sysInfo;
	DWORD ThreadId;
	SOCKET Accept;
	int ClientIndex;
	sockaddr_in cAddr;
	int cAddrlen = sizeof(cAddr);
	DWORD RecvBytes;
	DWORD Flags = 0;

	GetSystemInfo(&sysInfo);
	g_dwThreadCount = sysInfo.dwNumberOfProcessors * 2;

	// Cap at MAX_WORKER_THREAD
	if (g_dwThreadCount > MAX_WORKER_THREAD)
		g_dwThreadCount = MAX_WORKER_THREAD;
#ifdef __DEBUG_LOG
	g_LogProc.LogAddFunc("g_dwThreadCount set to %d", g_dwThreadCount);
#endif
	__try
	{
		// Create IOCP
		g_CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
#ifdef __DEBUG_LOG
		g_LogProc.LogAddFunc("IOCP created: %p", g_CompletionPort);
		g_LogProc.LogAddFunc("Creating %d IOCP worker threads", g_dwThreadCount);
#endif
		if (!g_CompletionPort)
		{
			EnterCriticalSection(&g_CS);
			g_LogProc.LogAddFuncColor(2, "CreateIoCompletionPort failed with error: %d", GetLastError());
			LeaveCriticalSection(&g_CS);
			__leave;
		}

		// Create IOCP worker threads
		for (DWORD dwCPU = 0; dwCPU < g_dwThreadCount; dwCPU++)
		{
			HANDLE hThread = CreateThread(NULL, 0, IOCPWorkerThread, g_CompletionPort, 0, &ThreadId);
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("IOCP worker thread %d created: handle=%p", dwCPU, hThread);
#endif
			if (!hThread)
			{
				EnterCriticalSection(&g_CS);
				g_LogProc.LogAddFuncColor(2, "CreateThread() failed with error %d", GetLastError());
				LeaveCriticalSection(&g_CS);
				__leave;
			}
			g_ThreadHandles[dwCPU] = hThread;
		}

		// Create listen socket
		if (!CreateListenSocket())
			__leave;
#ifdef __DEBUG_LOG
		g_LogProc.LogAddFunc("Listen socket created, entering accept loop");
#endif
		// Main accept loop
		while (TRUE)
		{
			Accept = WSAAccept(g_Listen, (sockaddr*)&cAddr, &cAddrlen, NULL, 0);

			if (Accept == SOCKET_ERROR)
			{
				EnterCriticalSection(&g_CS);
				int Error = WSAGetLastError();
				g_LogProc.LogAddFuncColor(2, "WSAAccept() failed with error %d", Error);
				GetLastErrorMsg();
				g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
				g_LogFileError.Output("WSAAccept() failed with error %d, %s\n", Error, ErrorMsg);
				g_LogListBoxError.Output("WSAAccept() failed with error %d, %s", Error, ErrorMsg);
				LeaveCriticalSection(&g_CS);
				__leave;
			}

			// Get IP
			in_addr cInAddr;
			memcpy(&cInAddr, &cAddr.sin_addr.s_addr, 4);

			// Add to connection pool
			ClientIndex = gObjAdd(Accept, &cInAddr);
			g_LogProc.LogAddFunc("Connect : IP [%s], ClientID = [%d]", inet_ntoa(cInAddr), ClientIndex);

			if (ClientIndex == -1)
			{
				closesocket(Accept);
				continue;
			}

			// Associate with IOCP
			if (!UpdateCompletionPort((HANDLE)Accept, ClientIndex))
			{
				EnterCriticalSection(&g_CS);
				GetLastErrorMsg();
				g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
				g_LogFileError.Output("CreateIoCompletionPort failed with error %d, %s\n", GetLastError(), ErrorMsg);
				g_LogProc.LogAddFuncColor(2, "CreateIoCompletionPort failed with error %d", GetLastError());
				LeaveCriticalSection(&g_CS);
				continue;
			}

			// Initialize RECV context
			PER_SOCKET_CONTEXT* pCtx = &gObj[ClientIndex].PerSocketContext;

			pCtx->IOContext[RECV_IO].wsabuf.buf = pCtx->IOContext[RECV_IO].Buffer;
			pCtx->IOContext[RECV_IO].wsabuf.len = MAX_BUFF_SIZE;
			pCtx->IOContext[RECV_IO].nTotalBytes = 0;
			pCtx->IOContext[RECV_IO].nSentBytes = 0;
			pCtx->IOContext[RECV_IO].nWaitIO = 0;
			pCtx->IOContext[RECV_IO].nSecondOfs = 0;
			pCtx->IOContext[RECV_IO].IOOperation = RECV_IO;

			// Initialize SEND context
			pCtx->IOContext[SEND_IO].wsabuf.buf = pCtx->IOContext[SEND_IO].Buffer;
			pCtx->IOContext[SEND_IO].wsabuf.len = MAX_BUFF_SIZE;
			pCtx->IOContext[SEND_IO].nTotalBytes = 0;
			pCtx->IOContext[SEND_IO].nSentBytes = 0;
			pCtx->IOContext[SEND_IO].nWaitIO = 0;
			pCtx->IOContext[SEND_IO].nSecondOfs = 0;
			pCtx->IOContext[SEND_IO].IOOperation = SEND_IO;

			pCtx->m_socket = Accept;
			pCtx->nIndex = ClientIndex;

			// Post initial receive
			int nRet = WSARecv(Accept, &pCtx->IOContext[RECV_IO].wsabuf, 1, &RecvBytes, &Flags,
				&pCtx->IOContext[RECV_IO].Overlapped, NULL);
#ifdef __DEBUG_LOG
			int nError = WSAGetLastError();  // SAVE IMMEDIATELY

			g_LogProc.LogAddFunc("WSARecv posted: ret=%d, error=%d, buf=%p, len=%d",
				nRet, nError, pCtx->IOContext[RECV_IO].wsabuf.buf, pCtx->IOContext[RECV_IO].wsabuf.len);
#endif

			if (nRet == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
			{
#ifdef __DEBUG_LOG
				g_LogProc.LogAddFunc("WSARecv FAILED: ret=%d, error=%d", nRet, nError);
#endif
				EnterCriticalSection(&g_CS);
				GetLastErrorMsg();
				int Error = WSAGetLastError();
				g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
				g_LogFileError.Output("IocpServerWorker :: WSARecv() failed with error %d, %s\n", Error, ErrorMsg);
				g_LogProc.LogAddFunc("IocpServerWorker :: WSARecv() failed with error %d", Error);
				g_LogProc.LogAddFunc("IocpServerWorker :: ClientIndex =  %d", ClientIndex);
				LeaveCriticalSection(&g_CS);

				pCtx->IOContext[RECV_IO].nWaitIO = 4;
				CloseClient(pCtx);
				continue;
			}
			else
			{
#ifdef __DEBUG_LOG
				g_LogProc.LogAddFunc("WSARecv OK: nWaitIO=1, IOCount incremented");
#endif
				pCtx->IOContext[RECV_IO].nWaitIO = 1;
				InterlockedIncrement(&pCtx->dwIOCount);
			}

			// Send connection success
			SCConnectResultSend(1, ClientIndex);
		}
	}
	__finally
	{
		// Shutdown: post completion packets
		if (g_CompletionPort)
		{
			for (DWORD i = 0; i < g_dwThreadCount; i++)
				PostQueuedCompletionStatus(g_CompletionPort, 0, 0, NULL);

			CloseHandle(g_CompletionPort);
			g_CompletionPort = NULL;
		}

		// Close listen socket
		if (g_Listen != INVALID_SOCKET)
		{
			closesocket(g_Listen);
			g_Listen = INVALID_SOCKET;
		}
	}

	return TRUE;
}
#else

DWORD WINAPI IocpServerWorker()
{
	DWORD ThreadId;
	SOCKET Accept;
	SYSTEM_INFO sysInfo;

	GetSystemInfo(&sysInfo);

	// Set thread count (2x CPU count)
	g_dwThreadCount = 2 * sysInfo.dwNumberOfProcessors;

	// Set PIM callback and start PIM worker threads
	// StartWorkers creates g_dwThreadCount * 4 worker threads internally
	g_cPIM.m_Callback = PIM_WORKERPROC;
	g_cPIM.StartWorkers(0);

	// Create IOCP completion port
	g_CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!g_CompletionPort)
	{
		EnterCriticalSection(&g_CS);
		//g_CS.Lock();
		g_LogProc.LogAddFuncColor(2, "CreateIoCompletionPort failed with error: %d", GetLastError());
		LeaveCriticalSection(&g_CS);
		//g_CS.UnLock();
		return 0;
	}

	// Create IOCP worker threads
	for (DWORD dwCPU = 0; dwCPU < g_dwThreadCount; ++dwCPU)
	{
		HANDLE hThread = CreateThread(NULL, 0, IOCPWorkerThread, g_CompletionPort, 0, &ThreadId);
#ifdef __DEBUG_LOG
		g_LogProc.LogAddFunc("IOCP worker thread %d created: handle=%p", dwCPU, hThread);
#endif
		if (!hThread)
		{
			EnterCriticalSection(&g_CS);
			//g_CS.Lock();
			g_LogProc.LogAddFuncColor(2, "CreateThread() failed with error %d", GetLastError());
			LeaveCriticalSection(&g_CS);
			//g_CS.UnLock();
			return 0;
		}
		g_ThreadHandles[dwCPU] = hThread;
	}

	// Create listen socket
	if (!CreateListenSocket())
		return 0;

	// Main accept loop
	while (!g_bDoTerminateLoopThread)
	{
		sockaddr_in addr = { 0 };
		int addrlen = sizeof(addr);

		Accept = WSAAccept(g_Listen, (sockaddr*)&addr, &addrlen, NULL, 0);

		if (Accept == INVALID_SOCKET)
		{
			EnterCriticalSection(&g_CS);
			//g_CS.Lock();
			int Error = WSAGetLastError();
			g_LogProc.LogAddFuncColor(2, "accept() failed with error %d", Error);
			GetLastErrorMsg();

			g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
			g_LogFileError.Output("accept() failed with error (%d, %s)\n", Error, ErrorMsg);
			g_LogListBoxError.Output("accept() failed with error (%d, %s)", Error, ErrorMsg);
			LeaveCriticalSection(&g_CS);
			//g_CS.UnLock();

			// Retry: recreate listen socket
			while (true)
			{
				closesocket(g_Listen);
				if (CreateListenSocket())
					break;

				int retryError = WSAGetLastError();
				g_LogProc.LogAddFuncColor(2, "Recreate Listen Socket is Failed. (ERROR : %d)  Retry After 60000 msec.", retryError);
				g_LogFileError.Output("Recreate Listen Socket is Failed. (ERROR : %d)", retryError);
				g_LogListBoxError.Output("Recreate Listen Socket is Failed. (ERROR : %d)  Retry After 60000 msec.", retryError);
				Sleep(60000);
			}

			g_LogProc.LogAddFuncColor(1, "Recreate Listen Socket is Succeed. (SID : %d)", g_Listen);
			g_LogFileError.Output("Recreate Listen Socket is Succeed. (SID : %d)", g_Listen);
			continue;
		}

		// ============================================================
		// 1. DDoS CHECK FIRST (original order)
		// ============================================================
		char szIP[29];
		memset(szIP, 0, sizeof(szIP));
		strcpy(szIP, inet_ntoa(*(struct in_addr*)&addr.sin_addr));

		IPADDR_INFO ipInfo = { 0 };
		g_CsIpAddrMng.GetIpAddrInfo(&ipInfo, (DWORD*)&addr.sin_addr);

		if (ipInfo.dwRestrictFlags == 1 && strcmp(szIP, DDOSNoCheckingIpAddress) != 0)
		{
			BOOL bDontLinger = TRUE;
			setsockopt(Accept, SOL_SOCKET, SO_DONTLINGER, (char*)&bDontLinger, sizeof(bDontLinger));
			closesocket(Accept);
			continue;
		}

		// ============================================================
		// 2. LOCAL NETWORK CHECK SECOND
		// ============================================================
		if (g_bLocalNetworkConnectOnly)
		{
			if (strcmp(szIP, "218.234.76.254") != 0 && strncmp(szIP, "172.30.120", 6) != 0)
			{
				g_LogFileError.Output("[Local Only #2] Block IP : %s ", szIP);
				sizeof(_PER_IO_CONTEXT);
				BOOL bDontLinger = TRUE;
				setsockopt(Accept, SOL_SOCKET, SO_DONTLINGER, (char*)&bDontLinger, sizeof(bDontLinger));
				closesocket(Accept);
				continue;
			}
		}

		// ============================================================
		// 3. ENQUEUE TO PIM
		// ============================================================
		int nQueueIndex = g_PIMQueueIndex % g_dwThreadCount;
		++g_PIMQueueIndex;
		if (nQueueIndex >= g_dwThreadCount)
			nQueueIndex = 0;

		_PER_PIM_CONTEXT* pData = new _PER_PIM_CONTEXT;
		pData->Socket = Accept;
		pData->IP = addr.sin_addr.s_addr;

		g_cPIM.Push(nQueueIndex, pData);
	}

	// Shutdown: post completion packets to wake worker threads
	if (g_CompletionPort)
	{
		for (DWORD k = 0; k < g_dwThreadCount; ++k)
			PostQueuedCompletionStatus(g_CompletionPort, 0, 0, NULL);

		CloseHandle(g_CompletionPort);
		g_CompletionPort = NULL;
	}

	// Close listen socket
	if (g_Listen != INVALID_SOCKET)
	{
		closesocket(g_Listen);
		g_Listen = INVALID_SOCKET;
	}

	return 1;
}
#endif
// =========================================================================
// IOCPRecvSignal - Handle Completed Receive
// =========================================================================
void IOCPRecvSignal(PER_SOCKET_CONTEXT* lpPerSocketContext, PER_IO_CONTEXT* lpIOContext, DWORD dwIoSize)
{
	lpIOContext->nSentBytes += dwIoSize;
	ParsingRecvData(lpIOContext, lpPerSocketContext->nIndex, lpPerSocketContext);

	lpIOContext->nWaitIO = 0;
	ZeroMemory(&lpIOContext->Overlapped, sizeof(OVERLAPPED));

	lpIOContext->wsabuf.len = MAX_BUFF_SIZE - lpIOContext->nSentBytes;
	lpIOContext->wsabuf.buf = lpIOContext->Buffer + lpIOContext->nSentBytes;
	lpIOContext->IOOperation = RECV_IO;

	DWORD recvBytes = 0, flags = 0;
	int ret = WSARecv(lpPerSocketContext->m_socket, &lpIOContext->wsabuf, 1, &recvBytes, &flags, &lpIOContext->Overlapped, NULL);

	if (ret == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		CloseClient(lpPerSocketContext);
		return;
	}

	InterlockedIncrement(&lpPerSocketContext->dwIOCount);
	lpIOContext->nWaitIO = 1;
}

// =========================================================================
// IOCPSendSignal - Handle Completed Send
// =========================================================================
void IOCPSendSignal(PER_SOCKET_CONTEXT* lpPerSocketContext, PER_IO_CONTEXT* lpIOContext, DWORD dwIoSize)
{
	int aIndex = lpPerSocketContext->nIndex;
	if (aIndex < 0 || aIndex >= MAX_CONNECTOBJECT)
		return;

	CCSLock cs(&gObj[aIndex].m_Lock);

	lpIOContext->nSentBytes += dwIoSize;

	if (lpIOContext->nSentBytes >= lpIOContext->nTotalBytes)
	{
		lpIOContext->nWaitIO = 0;
		if (lpIOContext->nSecondOfs > 0)
			IoSendSecond(lpPerSocketContext);
	}
	else
	{
		IoMoreSend(lpPerSocketContext);
	}
}

// =========================================================================
// ProcessIOCPSignal - Dispatch Completed IO
// =========================================================================
void ProcessIOCPSignal(DWORD ClientIndex, DWORD dwIoSize, PER_IO_CONTEXT* lpOverlapped)
{
	LPPER_SOCKET_CONTEXT	lpPerSocketContext = NULL;
	LPPER_IO_CONTEXT		lpIOContext = NULL;
	if (!lpOverlapped || ClientIndex >= MAX_CONNECTOBJECT)
		return;

	lpPerSocketContext = &gObj[ClientIndex].PerSocketContext;
	InterlockedDecrement(&lpPerSocketContext->dwIOCount);

	if (dwIoSize == 0)
	{
		CloseClient(lpPerSocketContext);
		return;
	}

	lpIOContext = (LPPER_IO_CONTEXT)lpOverlapped;
	if (lpIOContext->IOOperation == SEND_IO)
	{
		IOCPSendSignal(lpPerSocketContext, lpOverlapped, dwIoSize);
	}
	else if (lpIOContext->IOOperation == RECV_IO)
	{
		IOCPRecvSignal(lpPerSocketContext, lpOverlapped, dwIoSize);
	}
}

// =========================================================================
// IOCPWorkerThread - Worker Thread Entry Point
// =========================================================================
#ifdef __TEST
DWORD WINAPI IOCPWorkerThread(LPVOID lpThreadParameter)
{
#ifdef __DEBUG_LOG
	g_LogProc.LogAddFunc("IOCPWorkerThread STARTED");
#endif
	HANDLE hCompletionPort = (HANDLE)lpThreadParameter;

	while (TRUE)
	{
		DWORD dwIoSize = 0;
		ULONG_PTR ClientIndex = 0;
		LPOVERLAPPED lpOverlapped = NULL;

		BOOL bSuccess = GetQueuedCompletionStatus(
			hCompletionPort, &dwIoSize, (LPDWORD)&ClientIndex, &lpOverlapped, INFINITE);

		if (!bSuccess)
		{
			if (lpOverlapped)
			{
				DWORD errorcode = GetLastError();
				if (ERROR_NETNAME_DELETED != errorcode &&
					ERROR_CONNECTION_ABORTED != errorcode &&
					ERROR_OPERATION_ABORTED != errorcode &&
					ERROR_SEM_TIMEOUT != errorcode)
				{
					EnterCriticalSection(&g_CS);
					g_LogProc.LogAddFuncColor(2, "Error Thread : GetQueueCompletionStatus( %d )", errorcode);
					LeaveCriticalSection(&g_CS);
					return FALSE;
				}
			}
		}

		LPPER_SOCKET_CONTEXT lpPerSocketContext = &gObj[ClientIndex].PerSocketContext;
		lpPerSocketContext->dwIOCount--;

		if (dwIoSize == 0)
		{
			CloseClient(lpPerSocketContext);
			continue;
		}

		LPPER_IO_CONTEXT lpIOContext = (LPPER_IO_CONTEXT)lpOverlapped;

		if (lpIOContext->IOOperation == SEND_IO)
		{
			lpIOContext->nSentBytes += dwIoSize;
			if (lpIOContext->nSentBytes >= lpIOContext->nTotalBytes)
			{
				lpIOContext->nWaitIO = 0;
				if (lpIOContext->nSecondOfs > 0)
					IoSendSecond(lpPerSocketContext);
			}
			else
			{
				IoMoreSend(lpPerSocketContext);
			}
		}
		else if (lpIOContext->IOOperation == RECV_IO)
		{
			lpIOContext->nSentBytes += dwIoSize;
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("IOCP RECV: index=%d, ioSize=%d, totalSent=%d, firstByte=%02X",
				lpPerSocketContext->nIndex, dwIoSize, lpIOContext->nSentBytes,
				(BYTE)lpIOContext->Buffer[0]);
#endif
			ParsingRecvData(lpIOContext, lpPerSocketContext->nIndex, lpPerSocketContext);

			lpIOContext->nWaitIO = 0;
			ZeroMemory(&lpIOContext->Overlapped, sizeof(OVERLAPPED));

			lpIOContext->wsabuf.len = MAX_BUFF_SIZE - lpIOContext->nSentBytes;
			lpIOContext->wsabuf.buf = lpIOContext->Buffer + lpIOContext->nSentBytes;
			lpIOContext->IOOperation = RECV_IO;

			DWORD RecvBytes = 0;
			DWORD Flags = 0;
			int nRet = WSARecv(lpPerSocketContext->m_socket, &lpIOContext->wsabuf, 1,
				&RecvBytes, &Flags, &lpIOContext->Overlapped, NULL);

			if (nRet == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
			{
				g_LogProc.LogAddFunc("WSARecv() failed with error %d", WSAGetLastError());
				CloseClient(lpPerSocketContext);
				continue;
			}
			else
			{
				lpPerSocketContext->dwIOCount++;
				lpIOContext->nWaitIO = 1;
			}
		}
	}

	return TRUE;
}
#else

DWORD WINAPI IOCPWorkerThread(LPVOID lpThreadParameter)
{
	HANDLE	CompletionPort = (HANDLE)lpThreadParameter;
	DWORD	dwIoSize;

	DWORD	dwSendNumBytes = 0;
	BOOL	bSuccess = FALSE;
	int     ClientIndex;

	LPOVERLAPPED			lpOverlapped = NULL;

	while (TRUE)
	{
		bSuccess = GetQueuedCompletionStatus(
			CompletionPort,
			&dwIoSize,
			(LPDWORD)&ClientIndex,
			&lpOverlapped,
			INFINITE
		);
		// In IOCPWorkerThread, after GetQueuedCompletionStatus:
		if (ClientIndex == -1 && lpOverlapped == NULL) {
			// This is a shutdown signal
			break;  // Exit the thread
		}
		if (!bSuccess)
		{
			if (lpOverlapped != NULL)
			{
				DWORD Error = GetLastError();
				if (Error != ERROR_NETNAME_DELETED &&
					Error != ERROR_OPERATION_ABORTED)  // Fatal error
				{
					//CCSLock	cs(&g_CS);
					GetLastErrorMsg();
					g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
					g_LogFileError.Output("Error Thread : GetQueueCompletionStatus()  (%d, %s)\n", WSAGetLastError(), ErrorMsg);
					g_LogListBoxError.Output("Error Thread : GetQueueCompletionStatus()  (%d, %s)\n", WSAGetLastError(), ErrorMsg);
					g_LogProc.LogAddFuncColor(2, "Error Thread : GetQueueCompletionStatus (%d, %s)", WSAGetLastError(), ErrorMsg);
					return FALSE;
				}
			}
			// FIX: Skip processing if lpOverlapped is NULL
			if (lpOverlapped == NULL) {
				continue;  // Skip this packet
			}
		}
		// FIX: Also check here before processing
		if (lpOverlapped == NULL) {
			continue;
		}
		ProcessIOCPSignal(ClientIndex, dwIoSize, (LPPER_IO_CONTEXT)lpOverlapped);
	}
	return TRUE;
}
#endif

// =========================================================================
// ParsingRecvData - Parse Mu Online Protocol
// =========================================================================
int ParsingRecvData(PER_IO_CONTEXT* lpIOContext, int uIndex, PER_SOCKET_CONTEXT* lpPerSocketContext)
{
	LPBYTE recvBuf;
	int lOfs = 0;
	int packetSize = 0;
	BYTE headCode;

	if (lpIOContext->nSentBytes < 3)
	{
#ifdef __DEBUG_LOG
		g_LogProc.LogAddFunc("ParsingRecvData: not enough data (%d bytes)", lpIOContext->nSentBytes);
#endif
		return FALSE;
	}

	recvBuf = (LPBYTE)lpIOContext->Buffer;
#ifdef __DEBUG_LOG
	g_LogProc.LogAddFunc("ParsingRecvData: index=%d, bytes=%d, firstBytes=%02X %02X %02X %02X %02X",
		uIndex, lpIOContext->nSentBytes,
		recvBuf[0], recvBuf[1], recvBuf[2], recvBuf[3], recvBuf[4]);
#endif
	while (true)
	{
		if (recvBuf[lOfs] == PMHC_BYTE)
		{
			LPPBMSG_HEAD pHead	= (LPPBMSG_HEAD)(recvBuf + lOfs);
			packetSize			= pHead->size;
			headCode			= pHead->headcode;
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("Packet: C1 size=%d head=%02X", packetSize, headCode);
#endif
		}
		else if (recvBuf[lOfs] == PMHC_WORD)
		{
			LPPWMSG_HEAD pHead	= (LPPWMSG_HEAD)(recvBuf + lOfs);
			packetSize			= ((WORD)(pHead->sizeH) << 8);
			packetSize			|= (WORD)(pHead->sizeL);
			headCode			= pHead->headcode;
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("Packet: C2 size=%d head=%02X", packetSize, headCode);
#endif
		}
		else
		{
			// Invalid header
			EnterCriticalSection(&g_CS);
			//g_CS.Lock();
			g_LogFileError.Output("Invalid PacketHeader: Header:[%d], size:[%d] IP:[%s]\n",
				recvBuf[lOfs], lpIOContext->nSentBytes, gObj[uIndex].m_Ip);
			g_LogProc.LogAddFunc("Invalid PacketHeader: Header:[%d], size:[%d] IP:[%s]",
				recvBuf[lOfs], lpIOContext->nSentBytes, gObj[uIndex].m_Ip);
			LeaveCriticalSection(&g_CS);
			//g_CS.UnLock();

			// DDoS: track invalid packets
			DWORD clientIP = *reinterpret_cast<DWORD*>(&gObj[uIndex].m_IpBytes);
			if (g_CsIpAddrMng.IncIpAddrInvalidPacket(&clientIP) == 1 &&
				strcmp(gObj[uIndex].m_Ip, DDOSNoCheckingIpAddress) != 0)
			{
				struct linger ling = { 1, 0 };
				setsockopt(lpPerSocketContext->m_socket, SOL_SOCKET, SO_LINGER,
					reinterpret_cast<const char*>(&ling), sizeof(ling));

				g_LogProc.LogAddFuncColor(LOG_COLOR_RED,"[DDOS] Error: Abnormal Packet (Invalid Header) > %d (IP:%s)",
					g_dwMaxInvalidPackets, gObj[uIndex].m_Ip);

				g_LogFileError.Output("[DDOS] Error: Abnormal Packet (Invalid Header) > %d (IP:%s)",
					g_dwMaxInvalidPackets, gObj[uIndex].m_Ip);
			}
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("Invalid header: %02X at offset %d", recvBuf[lOfs], lOfs);
#endif
			lpIOContext->nSentBytes = 0;
			return 0;
		}

		if (packetSize <= 0)
		{
			EnterCriticalSection(&g_CS);
			//g_CS.Lock();
			g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
			g_LogFileError.Output("Error: (size <= 0) Packet size [%d]\n", packetSize);
			g_LogProc.LogAddFunc("error: Packet size [%d]", packetSize);
			LeaveCriticalSection(&g_CS);
			//g_CS.UnLock();
			return 0;
		}

		if (packetSize <= lpIOContext->nSentBytes)
		{
			// DDoS: track packets/sec
			DWORD clientIP = *reinterpret_cast<DWORD*>(&gObj[uIndex].m_IpBytes);
			if (g_CsIpAddrMng.IncIpAddrPacketPerSec(&clientIP) == 1 &&
				strcmp(gObj[uIndex].m_Ip, DDOSNoCheckingIpAddress) != 0)
			{
				struct linger ling = { 1, 0 };
				setsockopt(lpPerSocketContext->m_socket, SOL_SOCKET, SO_LINGER, reinterpret_cast<const char*>(&ling), sizeof(ling));

				g_LogProc.LogAddFuncColor(LOG_COLOR_RED,"[DDOS] Error: Packet/Sec > %d (IP:%s)", g_dwMaxPacketPerSec, gObj[uIndex].m_Ip);
				g_LogFileError.Output("[DDOS] Error: Packet/Sec > %d (IP:%s)", g_dwMaxPacketPerSec, gObj[uIndex].m_Ip);

				CloseClient(lpPerSocketContext);
				return 0;
			}
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("Calling ProtocolCore: head=%02X size=%d", headCode, packetSize);
#endif
			// Dispatch to protocol handler
			ProtocolCore(headCode, &recvBuf[lOfs], packetSize, static_cast<short>(uIndex));

			lOfs += packetSize;
			lpIOContext->nSentBytes -= packetSize;

			if (lpIOContext->nSentBytes <= 0)
				break;
		}
		else
		{
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("Incomplete packet: need %d have %d", packetSize, lpIOContext->nSentBytes);
#endif
			// Incomplete packet - shift remaining data
			if (lOfs > 0 && lpIOContext->nSentBytes > 0)
				memmove(recvBuf, &recvBuf[lOfs], lpIOContext->nSentBytes);
			break;
		}
	}

	return 1;
}

// =========================================================================
// IoSend - Send Data with Queuing
// =========================================================================
int IoSend(int aIndex, LPBYTE lpMsg, DWORD size)
{
	if (aIndex >= MAX_CONNECTOBJECT)
		return 0;

	PER_SOCKET_CONTEXT* pPerSocketContext = &gObj[aIndex].PerSocketContext;
	if (gObj[aIndex].Connected < 1)
		return 0;

	CCSLock cs(&gObj[aIndex].m_Lock);

	if (size > MAX_BUFF_SIZE)
	{
		EnterCriticalSection(&g_CS);
		//g_CS.Lock();
		g_LogFileError.Output("Error: File: [%s], Line: [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("Error: Max msg(%d) %s %d\n", size, __FILE__, __LINE__);

		g_LogProc.LogAddFuncColor(LOG_COLOR_GREEN, "Error: Max msg(%d) %s %d", size, __FILE__, __LINE__);
		LeaveCriticalSection(&g_CS);
		//g_CS.UnLock();
		CloseClient(aIndex);
		return 0;
	}

	PER_IO_CONTEXT* pSendContext = &pPerSocketContext->IOContext[SEND_IO];

	if (pSendContext->nWaitIO > 0)
	{
		if (pSendContext->nSecondOfs + size > MAX_BUFF_SIZE - 1)
		{
			EnterCriticalSection(&g_CS);
			//g_CS.Lock();
			g_LogFileError.Output("Error: File: [%s], Line: [%d]", __FILE__, __LINE__);
			g_LogFileError.Output("BUFFER OVER, IOSIZE=[%d] Total=[%d] Second=[%d]\n",
				size, pSendContext->nTotalBytes, pSendContext->nSecondOfs + size);

			g_LogProc.LogAddFunc("BUFFER OVER, IOSIZE=[%d] Total=[%d] Second=[%d]",
				size, pSendContext->nTotalBytes, pSendContext->nSecondOfs + size);
			LeaveCriticalSection(&g_CS);
			//g_CS.UnLock();

			pSendContext->nWaitIO = 0;
			CloseClient(aIndex);
			return 1;
		}
		else
		{
			memcpy(pSendContext->BufferSecond + pSendContext->nSecondOfs, lpMsg, size);
			pSendContext->nSecondOfs += size;
			return 1;
		}
	}

	pSendContext->nTotalBytes = 0;

	if (pSendContext->nSecondOfs > 0)
	{
		memcpy(pSendContext->Buffer, pSendContext->BufferSecond, pSendContext->nSecondOfs);
		pSendContext->nTotalBytes = pSendContext->nSecondOfs;
		pSendContext->nSecondOfs = 0;
	}

	if (pSendContext->nTotalBytes + size > MAX_BUFF_SIZE - 1)
	{
		EnterCriticalSection(&g_CS);
		//g_CS.Lock();
		g_LogFileError.Output("Error: File: [%s], Line: [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("error-L2_2 MAX BUFFER OVER\n");
		g_LogProc.LogAddFunc("error-L2_2 MAX BUFFER OVER");
		LeaveCriticalSection(&g_CS);
		//g_CS.UnLock();

		pSendContext->nWaitIO = 0;
		CloseClient(aIndex);
		return 0;
	}

	memcpy(pSendContext->Buffer + pSendContext->nTotalBytes, lpMsg, size);
	pSendContext->nTotalBytes += size;

	pSendContext->wsabuf.buf = pSendContext->Buffer;
	pSendContext->wsabuf.len = pSendContext->nTotalBytes;
	pSendContext->nSentBytes = 0;
	pSendContext->IOOperation = SEND_IO;

	DWORD sendBytes = 0;
	int ret = WSASend(pPerSocketContext->m_socket, &pSendContext->wsabuf, 1, &sendBytes, 0, &pSendContext->Overlapped, NULL);

	if (ret == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		EnterCriticalSection(&g_CS);
		//g_CS.Lock();
		GetLastErrorMsg();
		int Error = WSAGetLastError();
		g_LogFileError.Output("Error: File: [%s], Line: [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("WSASend() failed with error %d, %s\n", Error, ErrorMsg);
		if (Error != 10038)
			g_LogProc.LogAddFunc("WSASend() failed with error %d, %s", Error, ErrorMsg);
		LeaveCriticalSection(&g_CS);
		//g_CS.UnLock();

		CloseClient(aIndex);
		return 0;
	}

	InterlockedIncrement(&pPerSocketContext->dwIOCount);
	pSendContext->nWaitIO = 1;

	return 1;
}

// =========================================================================
// IoSendSecond - Send Queued Secondary Buffer
// =========================================================================
int IoSendSecond(PER_SOCKET_CONTEXT* lpPerSocketContext)
{
	int aIndex = lpPerSocketContext->nIndex;
	PER_IO_CONTEXT* pSendContext = &lpPerSocketContext->IOContext[SEND_IO];

	if (pSendContext->nWaitIO > 0 || pSendContext->nSecondOfs <= 0)
		return 0;

	memcpy(pSendContext->Buffer, pSendContext->BufferSecond, pSendContext->nSecondOfs);
	pSendContext->nTotalBytes = pSendContext->nSecondOfs;
	pSendContext->nSecondOfs = 0;

	pSendContext->wsabuf.buf = pSendContext->Buffer;
	pSendContext->wsabuf.len = pSendContext->nTotalBytes;
	pSendContext->nSentBytes = 0;
	pSendContext->IOOperation = SEND_IO;

	DWORD sendBytes = 0;
	int ret = WSASend(lpPerSocketContext->m_socket, &pSendContext->wsabuf, 1, &sendBytes, 0, &pSendContext->Overlapped, NULL);

	if (ret == SOCKET_ERROR && WSAGetLastError() != ERROR_IO_PENDING)
	{
		EnterCriticalSection(&g_CS);
		//g_CS.Lock();
		GetLastErrorMsg();
		int Error = WSAGetLastError();

		g_LogFileError.Output("Error: File: [%s], Line: [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("WSASend() failed with error %d, %s", Error, ErrorMsg);
		if (Error != 10038)
			g_LogProc.LogAddFunc("WSASend() failed with error %d, %s", Error, ErrorMsg);
		LeaveCriticalSection(&g_CS);
		//g_CS.UnLock();
		CloseClient(aIndex);
		return 0;
	}

	InterlockedIncrement(&lpPerSocketContext->dwIOCount);
	pSendContext->nWaitIO = 1;

	return 1;
}

// =========================================================================
// IoMoreSend - Send Remaining Data
// =========================================================================
int IoMoreSend(PER_SOCKET_CONTEXT* lpPerSocketContext)
{
	PER_IO_CONTEXT* pSendContext = &lpPerSocketContext->IOContext[SEND_IO];
	int remaining = pSendContext->nTotalBytes - pSendContext->nSentBytes;

	if (remaining <= 0)
		return 0;

	EnterCriticalSection(&g_CS);
	//g_CS.Lock();
	g_LogProc.LogAddFunc("%d Bytes Remain, Send Again", remaining);
	LeaveCriticalSection(&g_CS);
	//g_CS.UnLock();
	pSendContext->wsabuf.buf = pSendContext->Buffer + pSendContext->nSentBytes;
	pSendContext->wsabuf.len = remaining;
	pSendContext->IOOperation = SEND_IO;

	DWORD sendBytes = 0;
	int ret = WSASend(lpPerSocketContext->m_socket, &pSendContext->wsabuf, 1, &sendBytes, 0, &pSendContext->Overlapped, NULL);

	if (ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error == ERROR_IO_PENDING)
		{
			InterlockedIncrement(&lpPerSocketContext->dwIOCount);
			pSendContext->nWaitIO = 1;
			return 1;
		}

		EnterCriticalSection(&g_CS);
		//g_CS.Lock();
		GetLastErrorMsg();
		g_LogFileError.Output("Error: File: [%s], Line: [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("WSASend() failed with error %d, %s", Error, ErrorMsg);
		if (Error != 10038)
			g_LogProc.LogAddFunc("WSASend() failed with error %d, %s", Error, ErrorMsg);
		LeaveCriticalSection(&g_CS);
		//g_CS.UnLock();

		CloseClient(lpPerSocketContext->nIndex);
		return 0;
	}

	InterlockedIncrement(&lpPerSocketContext->dwIOCount);
	pSendContext->nWaitIO = 1;

	return 1;
}

// =========================================================================
// UpdateCompletionPort - Associate Socket with IOCP
// =========================================================================
BOOL UpdateCompletionPort(HANDLE FileHandle, ULONG_PTR CompletionKey)
{
	HANDLE IoCompletionPort;

	IoCompletionPort = CreateIoCompletionPort(FileHandle, g_CompletionPort, CompletionKey, 0);
#ifdef __DEBUG_LOG
	g_LogProc.LogAddFunc("Associate socket: h=%p, key=%d, result=%p",
		FileHandle, CompletionKey, IoCompletionPort);  
#endif
	if (IoCompletionPort)
	{
		g_CompletionPort = IoCompletionPort;
		InterlockedExchange(&gObj[CompletionKey].PerSocketContext.dwIOCount, 0);
		return TRUE;
	}

	EnterCriticalSection(&g_CS);
	//g_CS.Lock();
	GetLastErrorMsg();
	DWORD Error = GetLastError();

	g_LogFileError.Output("Error: File: [%s], Line: [%d]", __FILE__, __LINE__);
	g_LogFileError.Output("CreateIoCompletionPort: %d, %s\n", Error, ErrorMsg);

	g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "CreateIoCompletionPort Error: %d, %s", Error, ErrorMsg);
	g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "SOCKET ID: %d, ClientIndex = %d", FileHandle, CompletionKey);

	LeaveCriticalSection(&g_CS);
	//g_CS.UnLock();
	return FALSE;
}

// =========================================================================
// CloseClient - Cleanup Connection
// =========================================================================
void CloseClient(PER_SOCKET_CONTEXT* lpPerSocketContext)
{
	unsigned int index = lpPerSocketContext->nIndex;
	if (index >= MAX_CONNECTOBJECT)
		return;

	CCSLock cs(&gObj[index].m_Lock);

	if (gObj[index].m_socket != INVALID_SOCKET)
	{
		if (closesocket(gObj[index].m_socket) == SOCKET_ERROR)
		{
			if (WSAGetLastError() != 10038)  // Expected on close
				return;
		}
		gObj[index].m_socket = INVALID_SOCKET;
	}

	gObjDel(index);
}

void CloseClient(int index)
{
	if (index >= MAX_CONNECTOBJECT)
		return;

	CCSLock cs(&gObj[index].m_Lock);

	if (gObj[index].Connected != 0 && gObj[index].m_socket != INVALID_SOCKET)
	{
		closesocket(gObj[index].m_socket);
		gObj[index].m_socket = INVALID_SOCKET;
	}

	gObjDel(index);
}

void PIM_WORKERPROC(int WorkerID, int Param, _PER_PIM_CONTEXT* pData)
{
	if (!pData)
		return;

	SOCKET s = pData->Socket;

	struct in_addr addr;
	addr.s_addr = pData->IP;

	// Add to connection pool
	int nIndex = gObjAdd(s, &addr);


	if (nIndex == -1)
	{
		closesocket(s);
		return;
	}

	// Associate with IOCP
	if (!UpdateCompletionPort((HANDLE)s, nIndex))
	{
		g_LogProc.LogAddFuncColor(2, "CreateIoCompletionPort failed with error %d", GetLastError());
		return;
	}

	// Get the object's IO context
	PER_SOCKET_CONTEXT* pCtx = &gObj[nIndex].PerSocketContext;

	// Initialize RECV context
	pCtx->IOContext[RECV_IO].wsabuf.buf = pCtx->IOContext[RECV_IO].Buffer;
	pCtx->IOContext[RECV_IO].wsabuf.len = MAX_BUFF_SIZE;
	pCtx->IOContext[RECV_IO].nTotalBytes = 0;
	pCtx->IOContext[RECV_IO].nSentBytes = 0;
	pCtx->IOContext[RECV_IO].nWaitIO = 0;
	pCtx->IOContext[RECV_IO].nSecondOfs = 0;
	pCtx->IOContext[RECV_IO].IOOperation = RECV_IO;

	// Initialize SEND context
	pCtx->IOContext[SEND_IO].wsabuf.buf = pCtx->IOContext[SEND_IO].Buffer;
	pCtx->IOContext[SEND_IO].wsabuf.len = MAX_BUFF_SIZE;
	pCtx->IOContext[SEND_IO].nTotalBytes = 0;
	pCtx->IOContext[SEND_IO].nSentBytes = 0;
	pCtx->IOContext[SEND_IO].nWaitIO = 0;
	pCtx->IOContext[SEND_IO].nSecondOfs = 0;
	pCtx->IOContext[SEND_IO].IOOperation = SEND_IO;

	// Store socket and index
	pCtx->m_socket = s;
	pCtx->nIndex = nIndex;

	// Post initial receive
	DWORD recvBytes = 0, flags = 0;
	int ret = WSARecv(
		s,
		&pCtx->IOContext[RECV_IO].wsabuf,
		1,
		&recvBytes,
		&flags,
		&pCtx->IOContext[RECV_IO].Overlapped,
		NULL);
#ifdef __DEBUG_LOG
	g_LogProc.LogAddFunc("=== Object Layout Debug ===");
	g_LogProc.LogAddFunc("OBJECTSTRUCT size: %d (need 24748)", sizeof(OBJECTSTRUCT));
	g_LogProc.LogAddFunc("PER_IO_CONTEXT size: %d", sizeof(PER_IO_CONTEXT));
	g_LogProc.LogAddFunc("PER_SOCKET_CONTEXT size: %d", sizeof(PER_SOCKET_CONTEXT));
	g_LogProc.LogAddFunc("IOContext[0] buf: %p", pCtx->IOContext[0].wsabuf.buf);
	g_LogProc.LogAddFunc("IOContext[1] buf: %p", pCtx->IOContext[1].wsabuf.buf);
	g_LogProc.LogAddFunc("IOContext[0] len: %d", pCtx->IOContext[0].wsabuf.len);
	g_LogProc.LogAddFunc("IOContext[1] len: %d", pCtx->IOContext[1].wsabuf.len);
	g_LogProc.LogAddFunc("PerSocketContext offset in obj: %d", (int)((char*)pCtx - (char*)&gObj[nIndex]));
#endif
	int nError = WSAGetLastError();

	if (ret != SOCKET_ERROR || nError == WSA_IO_PENDING)
	{
		pCtx->IOContext[RECV_IO].nWaitIO = 1;
		InterlockedIncrement(&pCtx->dwIOCount);
		SCConnectResultSend(1, nIndex);
	}
	else
	{
		g_LogProc.LogAddFunc("IocpServerWorker :: WSARecv() failed with error %d", nError);
		g_LogProc.LogAddFunc("IocpServerWorker :: ClientIndex =  %d", nIndex);
		pCtx->IOContext[RECV_IO].nWaitIO = 4;
		CloseClient(pCtx);
	}
}