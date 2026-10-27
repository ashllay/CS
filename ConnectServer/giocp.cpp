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
CRITICAL_SECTION	g_critERROR_LOG;// = { NULL, 0, 0, NULL, NULL, 0 };
CPIM				g_cPIM(3);
CCriticalSec		g_CS;
int					g_ServerPort = 0;
// See MAX_WORKER_THREAD_STORAGE in giocp.h: the original array only holds
// MAX_WORKER_THREAD (16) entries while IocpServerWorker() stores
// g_dwThreadCount = 2 * dwNumberOfProcessors handles, overflowing into ErrorMsg
// in the original .data layout.  Recording all of them (instead of overflowing)
// reproduces the original's effective behaviour - DestroyGIocp() sees the same
// handles - without relying on where the linker places the neighbouring
// globals.
HANDLE				g_ThreadHandles[MAX_WORKER_THREAD_STORAGE];
char				ErrorMsg[1024];
DWORD				g_dwThreadCount = 0;
HANDLE				g_CompletionPort = NULL;
LONG				g_iSockConnNum = 0;

// =========================================================================
// GetLastErrorMsg - Format Winsock Error Message
// =========================================================================
void GetLastErrorMsg()
{
	EnterCriticalSection(&g_critERROR_LOG);

	// Original (0x401080) formats directly into the global ErrorMsg buffer
	// and then force-terminates the last byte.
	DWORD LastError = GetLastError();
	FormatMessageA(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		LastError,
		0,
		ErrorMsg,
		1024,
		NULL);
	ErrorMsg[1023] = 0;

	LeaveCriticalSection(&g_critERROR_LOG);
}

// =========================================================================
// CreateGIocp - Initialize IOCP Server
// =========================================================================
int CreateGIocp(int server_port)
{
	gObjInit();              // Initialize connection pool
	g_ServerPort = server_port;
	IocpServerWorker();
	return 1;
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
	sockaddr addr;

	// Create overlapped socket
	g_Listen = WSASocketA(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (g_Listen == INVALID_SOCKET)
	{
		g_CS.Lock();
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "WSASocket() failed with error %d", WSAGetLastError());
		g_CS.UnLock();
		return FALSE;
	}

	// Bind to port
	addr.sa_family = AF_INET;
	*(DWORD*)&addr.sa_data[2] = htonl(INADDR_ANY);
	*(WORD*)addr.sa_data = htons(static_cast<u_short>(g_ServerPort));

	if (bind(g_Listen, &addr, sizeof(addr)) == SOCKET_ERROR)
	{
		// NOTE: The original does not lock g_CS here.
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED,
			"bind error (It seems like Game Server is Running Overlapped with Same ID. Please Close That Game Server)");
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

DWORD IocpServerWorker()
{
	DWORD ThreadId;
	SOCKET Accept;
	SYSTEM_INFO SystemInfo;
	sockaddr addr;			// NOTE: intentionally not initialized (original reuses it)
	int addrlen;

	GetSystemInfo(&SystemInfo);

	// Set thread count (2x CPU count)
	g_dwThreadCount = 2 * SystemInfo.dwNumberOfProcessors;

	// Set PIM callback and start PIM worker threads
	// StartWorkers creates 4 worker threads per queue internally
	g_cPIM.m_Callback = PIM_WORKERPROC;
	g_cPIM.StartWorkers();

	// Create IOCP completion port
	g_CompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (!g_CompletionPort)
	{
		g_CS.Lock();
		// NOTE: The original format string expects (error, description) but
		// only the error code is passed; the %s reads the stale stack value.
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "CreateIoCompletionPort: %d, %s\n", GetLastError());
		g_CS.UnLock();
	}
	else
	{
		bool bCreateThreadFailed = false;

		// Create IOCP worker threads
		for (DWORD dwCPU = 0; dwCPU < g_dwThreadCount; ++dwCPU)
		{
			HANDLE hThread = CreateThread(NULL, 0, IOCPWorkerThread, g_CompletionPort, 0, &ThreadId);
			if (!hThread)
			{
				g_CS.Lock();
				g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "CreateThread() failed with error %d", GetLastError());
				g_CS.UnLock();
				bCreateThreadFailed = true;
				break;
			}
			g_ThreadHandles[dwCPU] = hThread;
		}

		// Create listen socket
		if (!bCreateThreadFailed && CreateListenSocket())
		{
			// Main accept loop
			while (!g_bDoTerminateLoopThread)
			{
				addrlen = 16;

				Accept = WSAAccept(g_Listen, &addr, &addrlen, NULL, 0);

				if (Accept == INVALID_SOCKET)
				{
					g_CS.Lock();
					g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "accept() failed with error %d", WSAGetLastError());
					GetLastErrorMsg();
					g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
					g_LogFileError.Output("accept() failed with error (%d, %s)\n", WSAGetLastError(), ErrorMsg);
					g_LogListBoxError.Output("accept() failed with error (%d, %s)", WSAGetLastError(), ErrorMsg);
					g_CS.UnLock();

					// Retry: recreate listen socket
					while (true)
					{
						closesocket(g_Listen);
						if (CreateListenSocket())
							break;

						int retryError = WSAGetLastError();
						g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "Recreate Listen Socket is Failed. (ERROR : %d)  Retry After 60000 msec.", retryError);
						g_LogFileError.Output("Recreate Listen Socket is Failed. (ERROR : %d)", retryError);
						g_LogListBoxError.Output("Recreate Listen Socket is Failed. (ERROR : %d)  Retry After 60000 msec.", retryError);
						Sleep(60000);
					}

					g_LogProc.LogAddFuncColor(1, "Recreate Listen Socket is Succeed. (SID : %d)", g_Listen);
					g_LogFileError.Output("Recreate Listen Socket is Succeed. (SID : %d)", g_Listen);

					// NOTE: The original does NOT 'continue' here. It falls through
					// with Accept == INVALID_SOCKET and the stale sockaddr values.
				}

				// ============================================================
				// 1. LOCAL NETWORK CHECK (original order)
				// ============================================================
				if (g_bLocalNetworkConnectOnly)
				{
					char szIP[29];
					memset(szIP, 0, sizeof(szIP));
					strcpy(szIP, inet_ntoa(*(struct in_addr*)&addr.sa_data[2]));

					if (strcmp(szIP, "218.234.76.254") != 0 && strncmp(szIP, "172.30.120", 6) != 0)
					{
						g_LogFileError.Output("[Local Only #2] Block IP : %s ", szIP);
						struct linger ling = { 1, 0 };
						setsockopt(Accept, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));
						closesocket(Accept);
						continue;
					}
				}

				// ============================================================
				// 2. DDOS CHECK
				// ============================================================
				IPADDR_INFO ipInfo = g_CsIpAddrMng.GetIpAddrInfo(*(DWORD*)&addr.sa_data[2]);

				char szIP[29];
				memset(szIP, 0, sizeof(szIP));
				strcpy(szIP, inet_ntoa(*(struct in_addr*)&addr.sa_data[2]));

				if (ipInfo.dwRestricted != 1 || strcmp(szIP, DDOSNoCheckingIpAddress) == 0)
				{
					// ============================================================
					// 3. ENQUEUE TO PIM
					// ============================================================
					int nQueueIndex = g_cPIM.m_QueueIndex % g_cPIM.m_QueueCount;
					++g_cPIM.m_QueueIndex;
					if (nQueueIndex >= g_cPIM.m_QueueCount)
						nQueueIndex = 0;

					_PER_PIM_CONTEXT* pData = new _PER_PIM_CONTEXT;
					pData->Socket = Accept;
					pData->IpAddr = *(struct in_addr*)&addr.sa_data[2];

					g_cPIM.Push(nQueueIndex, pData);
				}
				else
				{
					struct linger ling = { 1, 0 };
					setsockopt(Accept, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));
					closesocket(Accept);
				}
			}
		}
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
		CloseClient(lpPerSocketContext, 0);
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
		CloseClient(lpPerSocketContext, 0);
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
DWORD WINAPI IOCPWorkerThread(LPVOID lpThreadParameter)
{
	HANDLE	CompletionPort = (HANDLE)lpThreadParameter;
	DWORD	dwIoSize = 0;
	DWORD	ClientIndex = 0;

	LPOVERLAPPED			lpOverlapped = NULL;

	for (;;)
	{
		if (!GetQueuedCompletionStatus(
			CompletionPort,
			&dwIoSize,
			&ClientIndex,
			&lpOverlapped,
			INFINITE))
		{
			if (lpOverlapped != NULL)
			{
				DWORD Error = GetLastError();
				if (Error != ERROR_NETNAME_DELETED &&
					Error != ERROR_OPERATION_ABORTED)
					break;
			}
		}

		// NOTE: The original calls ProcessIOCPSignal even when
		// GetQueuedCompletionStatus fails with a deleted/aborted connection.
		ProcessIOCPSignal(ClientIndex, dwIoSize, (LPPER_IO_CONTEXT)lpOverlapped);
	}

	// Fatal completion-port error: log and terminate this worker.
	g_CS.Lock();
	GetLastErrorMsg();
	g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
	g_LogFileError.Output("Error Thread : GetQueueCompletionStatus()  (%d, %s)\n", GetLastError(), ErrorMsg);
	g_LogListBoxError.Output("Error Thread : GetQueueCompletionStatus()  (%d, %s)\n", GetLastError(), ErrorMsg);
	g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "Error Thread : GetQueueCompletionStatus (%d, %s)", GetLastError(), ErrorMsg);
	g_CS.UnLock();

	return 0;
}

// =========================================================================
// ParsingRecvData - Parse Mu Online Protocol
// =========================================================================
int ParsingRecvData(PER_IO_CONTEXT* lpIOContext, int uIndex, PER_SOCKET_CONTEXT* lpPerSocketContext)
{
	LPBYTE recvBuf;
	int packetSize = 0;
	BYTE headCode = 0;
	BYTE PacketBuffer[MAX_BUFF_SIZE];

	if (lpIOContext->nSentBytes < 3)
	{
#ifdef __DEBUG_LOG
		g_LogProc.LogAddFunc("ParsingRecvData: not enough data (%d bytes)", lpIOContext->nSentBytes);
#endif
		return FALSE;
	}

	recvBuf = (LPBYTE)lpIOContext->Buffer;
	memset(PacketBuffer, 0, sizeof(PacketBuffer));

#ifdef __DEBUG_LOG
	g_LogProc.LogAddFunc("ParsingRecvData: index=%d, bytes=%d, firstBytes=%02X %02X %02X %02X %02X",
		uIndex, lpIOContext->nSentBytes,
		recvBuf[0], recvBuf[1], recvBuf[2], recvBuf[3], recvBuf[4]);
#endif
	while (true)
	{
		if (recvBuf[0] == PMHC_BYTE)
		{
			packetSize = recvBuf[1];
			headCode = recvBuf[2];
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("Packet: C1 size=%d head=%02X", packetSize, headCode);
#endif
		}
		else if (recvBuf[0] == PMHC_WORD)
		{
			packetSize = (recvBuf[1] << 8) | recvBuf[2];
			headCode = recvBuf[3];
#ifdef __DEBUG_LOG
			g_LogProc.LogAddFunc("Packet: C2 size=%d head=%02X", packetSize, headCode);
#endif
		}
		else
		{
			// Invalid header
			g_CS.Lock();
			g_LogFileError.Output("Invalid PacketHeader: Header:[%d], size:[%d] IP:[%s]\n",
				recvBuf[0], lpIOContext->nSentBytes, gObj[uIndex].m_Ip);
			g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "Invalid PacketHeader : Header:[%d], size:[%d] IP:[%s]",
				recvBuf[0], lpIOContext->nSentBytes, gObj[uIndex].m_Ip);
			g_CS.UnLock();

			lpIOContext->nSentBytes = 0;

			// DDoS: track invalid packets
			if (g_CsIpAddrMng.IncIpAddrInvalidPacket(gObj[uIndex].m_IpDword) == 1 &&
				strcmp(gObj[uIndex].m_Ip, DDOSNoCheckingIpAddress) != 0)
			{
				struct linger ling = { 1, 0 };
				setsockopt(lpPerSocketContext->m_socket, SOL_SOCKET, SO_LINGER,
					(char*)&ling, sizeof(ling));
				g_LogProc.LogAddFuncColor(LOG_COLOR_RED,
					"[DDOS] Error : Abnormal Packet (Invalid Packet Header) > %d (IP:%d.%d.%d.%d)",
					g_dwMaxInvalidPackets, gObj[uIndex].m_IpBytes[0], gObj[uIndex].m_IpBytes[1],
					gObj[uIndex].m_IpBytes[2], gObj[uIndex].m_IpBytes[3]);
				g_LogFileError.Output(
					"[DDOS] Error : Abnormal Packet (Invalid Packet Header) > %d (IP:%d.%d.%d.%d)",
					g_dwMaxInvalidPackets, gObj[uIndex].m_IpBytes[0], gObj[uIndex].m_IpBytes[1],
					gObj[uIndex].m_IpBytes[2], gObj[uIndex].m_IpBytes[3]);
				CloseClient(lpPerSocketContext, 0);
			}
			return 0;
		}

		if (packetSize <= 0)
		{
			g_CS.Lock();
			g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
			g_LogFileError.Output("Error : (size <= 0) Packet size [%d]\n", packetSize);
			g_LogProc.LogAddFunc("error : Packet size [%d]", packetSize);
			g_CS.UnLock();
			return 0;
		}

		// NOTE: The original simply returns on an incomplete packet; the
		// remaining bytes are left in place for the next WSARecv.
		if (packetSize > lpIOContext->nSentBytes)
			return 0;

		// DDoS: track packets/sec
		if (g_CsIpAddrMng.IncIpAddrPacketPerSec(gObj[uIndex].m_IpDword) == 1 &&
			strcmp(gObj[uIndex].m_Ip, DDOSNoCheckingIpAddress) != 0)
		{
			struct linger ling = { 1, 0 };
			setsockopt(lpPerSocketContext->m_socket, SOL_SOCKET, SO_LINGER, (char*)&ling, sizeof(ling));

			g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "[DDOS] Error : Packet/Sec > %d (IP:%d.%d.%d.%d)",
				g_dwMaxPacketPerSec, gObj[uIndex].m_IpBytes[0], gObj[uIndex].m_IpBytes[1],
				gObj[uIndex].m_IpBytes[2], gObj[uIndex].m_IpBytes[3]);
			g_LogFileError.Output("[DDOS] Error : Packet/Sec > %d (IP:%d.%d.%d.%d)",
				g_dwMaxPacketPerSec, gObj[uIndex].m_IpBytes[0], gObj[uIndex].m_IpBytes[1],
				gObj[uIndex].m_IpBytes[2], gObj[uIndex].m_IpBytes[3]);

			CloseClient(lpPerSocketContext, 0);
			return 0;
		}

#ifdef __DEBUG_LOG
		g_LogProc.LogAddFunc("Calling ProtocolCore: head=%02X size=%d", headCode, packetSize);
#endif
		memcpy(PacketBuffer, recvBuf, packetSize);
		memmove(recvBuf, recvBuf + packetSize, lpIOContext->nSentBytes - packetSize);
		lpIOContext->nSentBytes -= packetSize;

		// Dispatch to protocol handler
		ProtocolCore(headCode, PacketBuffer, packetSize, static_cast<short>(uIndex));
	}
	return 0;
}

// =========================================================================
// IoSend - Send Data with Queuing
// =========================================================================
int IoSend(int aIndex, LPBYTE lpMsg, DWORD size)
{
	if (aIndex < 0 || aIndex > MAX_CONNECTOBJECT - 1)
		return 0;

	PER_SOCKET_CONTEXT* pPerSocketContext = &gObj[aIndex].PerSocketContext;
	if (gObj[aIndex].Connected < 1)
		return 0;

	CCSLock cs(&gObj[aIndex].m_Lock);

	if (size > MAX_BUFF_SIZE)
	{
		g_CS.Lock();
		g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("Error : Max msg(%d) %s %d\n", size, __FILE__, __LINE__);
		g_LogProc.LogAddFuncColor(LOG_COLOR_GREEN, "Error : Max msg(%d) %s %d", size, __FILE__, __LINE__);
		g_CS.UnLock();
		CloseClient(aIndex);
		return 0;
	}

	PER_IO_CONTEXT* pSendContext = &pPerSocketContext->IOContext[SEND_IO];

	if (pSendContext->nWaitIO > 0)
	{
		if (pSendContext->nSecondOfs + size > MAX_BUFF_SIZE - 1)
		{
			g_CS.Lock();
			g_LogProc.LogAddFunc("BUFFER OVER, IOSIZE = [%d] Total Size = [%d] Second Size = [%d]",
				size, pSendContext->nTotalBytes, pSendContext->nSecondOfs + size);
			g_CS.UnLock();

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
		g_CS.Lock();
		g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("error-L2_2 MAX BUFFER OVER (IP:%s)\n", gObj[aIndex].m_Ip);
		g_LogProc.LogAddFunc("error-L2_2 MAX BUFFER OVER (IP:%s)", gObj[aIndex].m_Ip);
		g_CS.UnLock();

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

	if (ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != ERROR_IO_PENDING)
		{
			g_CS.Lock();
			GetLastErrorMsg();
			g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
			g_LogFileError.Output("WSASend() failed with error %d, %s\n", Error, ErrorMsg);
			if (Error != 10038)
				g_LogProc.LogAddFunc("WSASend() failed with error %d, %s", Error, ErrorMsg);
			g_CS.UnLock();

			CloseClient(aIndex);
			return 0;
		}
	}
	else
	{
		// NOTE: The original increments dwIOCount only when WSASend completes
		// synchronously; a pending (WSA_IO_PENDING) send is not counted.
		InterlockedIncrement(&pPerSocketContext->dwIOCount);
	}

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

	if (pSendContext->nWaitIO > 0)
		return 0;

	pSendContext->nTotalBytes = 0;

	if (pSendContext->nSecondOfs <= 0)
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

	if (ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != ERROR_IO_PENDING)
		{
			g_CS.Lock();
			GetLastErrorMsg();
			g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
			g_LogFileError.Output("WSASend() failed with error %d, %s", Error, ErrorMsg);
			if (Error != 10038)
				g_LogProc.LogAddFunc("WSASend() failed with error %d, %s", Error, ErrorMsg);
			g_CS.UnLock();
			CloseClient(aIndex);
			return 0;
		}
	}
	else
	{
		// NOTE: Same original quirk as IoSend - pending sends do not increment
		// the pending-IO counter.
		InterlockedIncrement(&lpPerSocketContext->dwIOCount);
	}

	pSendContext->nWaitIO = 1;

	return 1;
}

// =========================================================================
// IoMoreSend - Send Remaining Data
// =========================================================================
int IoMoreSend(PER_SOCKET_CONTEXT* lpPerSocketContext)
{
	PER_IO_CONTEXT* pSendContext = &lpPerSocketContext->IOContext[SEND_IO];
	int aIndex = lpPerSocketContext->nIndex;

	if (pSendContext->nTotalBytes - pSendContext->nSentBytes < 0)
		return 0;

	g_CS.Lock();
	g_LogProc.LogAddFunc("%d Bytes Remain, Send Again", pSendContext->nTotalBytes - pSendContext->nSentBytes);
	g_CS.UnLock();

	pSendContext->wsabuf.buf = pSendContext->Buffer + pSendContext->nSentBytes;
	pSendContext->wsabuf.len = pSendContext->nTotalBytes - pSendContext->nSentBytes;
	pSendContext->IOOperation = SEND_IO;

	DWORD sendBytes = 0;
	int ret = WSASend(lpPerSocketContext->m_socket, &pSendContext->wsabuf, 1, &sendBytes, 0, &pSendContext->Overlapped, NULL);

	if (ret == SOCKET_ERROR)
	{
		int Error = WSAGetLastError();
		if (Error != ERROR_IO_PENDING)
		{
			g_CS.Lock();
			GetLastErrorMsg();
			g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
			g_LogFileError.Output("WSASend() failed with error %d, %s", Error, ErrorMsg);
			if (Error != 10038)
				g_LogProc.LogAddFunc("WSASend() failed with error %d, %s", Error, ErrorMsg);
			g_CS.UnLock();

			CloseClient(aIndex);
			return 0;
		}
	}
	else
	{
		// NOTE: Same original quirk - only synchronous completion increments
		// the pending-IO counter.
		InterlockedIncrement(&lpPerSocketContext->dwIOCount);
	}

	pSendContext->nWaitIO = 1;

	return 1;
}

// =========================================================================
// UpdateCompletionPort - Associate Socket with IOCP
// =========================================================================
int UpdateCompletionPort(SOCKET s, int nIndex, int bAdd)
{
	HANDLE IoCompletionPort;

	IoCompletionPort = CreateIoCompletionPort((HANDLE)s, g_CompletionPort, nIndex, 0);
#ifdef __DEBUG_LOG
	g_LogProc.LogAddFunc("Associate socket: h=%p, key=%d, result=%p",
		s, nIndex, IoCompletionPort);
#endif
	if (IoCompletionPort)
	{
		g_CompletionPort = IoCompletionPort;
		InterlockedExchange(&gObj[nIndex].PerSocketContext.dwIOCount, 0);
		return 1;
	}

	g_CS.Lock();
	GetLastErrorMsg();
	g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
	g_LogFileError.Output("CreateIoCompletionPort: %d, %s\n", GetLastError(), ErrorMsg);
	g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "CreateIoCompletionPort Error : %d, %s", GetLastError(), ErrorMsg);
	g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "SOCKET ID : %d, ClientIndex = %d", s, nIndex);
	g_CS.UnLock();

	return 0;
}

// =========================================================================
// CloseClient - Cleanup Connection
// =========================================================================
void CloseClient(PER_SOCKET_CONTEXT* lpPerSocketContext, int bAdd)
{
	int index = lpPerSocketContext->nIndex;
	if (index < 0 || index > MAX_CONNECTOBJECT - 1)
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
	if (index < 0 || index > MAX_CONNECTOBJECT - 1)
		return;

	CCSLock cs(&gObj[index].m_Lock);

	// NOTE: Unlike CloseClient(PER_SOCKET_CONTEXT*, int), the original does not
	// call gObjDel() here; the connection object is released later by the IOCP path.
	if (gObj[index].Connected != 0)
	{
		closesocket(gObj[index].m_socket);
		gObj[index].m_socket = INVALID_SOCKET;
	}
}

void PIM_WORKERPROC(int WorkerID, int Param, void* pData)
{
	_PER_PIM_CONTEXT* lpPerPim = (_PER_PIM_CONTEXT*)pData;

	if (lpPerPim == NULL)
		return;

	SOCKET s = lpPerPim->Socket;

	int index = gObjAdd(s, lpPerPim->IpAddr);

	if (index == -1)
	{
		closesocket(s);
		return;
	}

	if (!UpdateCompletionPort(s, index, 1))
	{
		g_CS.Lock();
		GetLastErrorMsg();
		g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("CreateIoCompletionPort failed with error %d, %s\n", GetLastError(), ErrorMsg);
		g_LogProc.LogAddFuncColor(LOG_COLOR_RED, "CreateIoCompletionPort failed with error %d", GetLastError());
		g_CS.UnLock();
		return;
	}

	PER_SOCKET_CONTEXT* lpSocket = &gObj[index].PerSocketContext;

	// RECV context
	lpSocket->IOContext[RECV_IO].wsabuf.buf = lpSocket->IOContext[RECV_IO].Buffer;
	lpSocket->IOContext[RECV_IO].wsabuf.len = MAX_BUFF_SIZE;
	lpSocket->IOContext[RECV_IO].nTotalBytes = 0;
	lpSocket->IOContext[RECV_IO].nSentBytes = 0;
	lpSocket->IOContext[RECV_IO].nWaitIO = 0;
	lpSocket->IOContext[RECV_IO].nSecondOfs = 0;
	lpSocket->IOContext[RECV_IO].IOOperation = RECV_IO;

	// SEND context
	lpSocket->IOContext[SEND_IO].wsabuf.buf = lpSocket->IOContext[SEND_IO].Buffer;
	lpSocket->IOContext[SEND_IO].wsabuf.len = MAX_BUFF_SIZE;
	lpSocket->IOContext[SEND_IO].nTotalBytes = 0;
	lpSocket->IOContext[SEND_IO].nSentBytes = 0;
	lpSocket->IOContext[SEND_IO].nWaitIO = 0;
	lpSocket->IOContext[SEND_IO].nSecondOfs = 0;
	lpSocket->IOContext[SEND_IO].IOOperation = SEND_IO;

	lpSocket->m_socket = s;
	lpSocket->nIndex = index;

	DWORD recvBytes = 0;
	DWORD flags = 0;

	if (WSARecv(
		s,
		&lpSocket->IOContext[RECV_IO].wsabuf,
		1,
		&recvBytes,
		&flags,
		&lpSocket->IOContext[RECV_IO].Overlapped,
		NULL) != SOCKET_ERROR
		|| WSAGetLastError() == WSA_IO_PENDING)
	{
		lpSocket->IOContext[RECV_IO].nWaitIO = 1;
		++lpSocket->dwIOCount;
		SCConnectResultSend(1, index);
	}
	else
	{
		g_CS.Lock();
		GetLastErrorMsg();
		g_LogFileError.Output("Error : File : [%s], Line : [%d]", __FILE__, __LINE__);
		g_LogFileError.Output("IocpServerWorker :: WSARecv() failed with error %d, %s\n", WSAGetLastError(), ErrorMsg);
		g_LogProc.LogAddFunc("IocpServerWorker :: WSARecv() failed with error %d", WSAGetLastError());
		g_LogProc.LogAddFunc("IocpServerWorker :: ClientIndex =  %d", index);
		g_CS.UnLock();
		lpSocket->IOContext[RECV_IO].nWaitIO = 4;
		CloseClient(lpSocket, 0);
	}
}