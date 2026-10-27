#pragma once

#ifndef __GIOCP_H__
#define __GIOCP_H__
#include "StdAfx.h"
#include "CPIM.h"

// Constants
#define MAX_BUFF_SIZE       6144
#define MAX_WORKER_THREAD   16                // Max IOCP threads
#define MAX_CONNECTOBJECT   10000             // Max connections (0x2710)

// QUIRK / LATENT BUG (kept from the original):
// IocpServerWorker() creates g_dwThreadCount = 2 * dwNumberOfProcessors worker
// threads and stores every handle in g_ThreadHandles, but the original only
// reserved MAX_WORKER_THREAD (16) entries.  With more than eight logical CPUs
// the original therefore writes past the end of the array.  In the original
// build ErrorMsg sits immediately after the array in .data (g_ThreadHandles
// 0x43869C, ErrorMsg 0x4386DC), so the surplus handles overwrite ErrorMsg[0..]
// and DestroyGIocp() reads the very same values back - harmless by accident.
// The v90 toolset lays the zero-initialized globals out differently (ErrorMsg
// before the array, g_critERROR_LOG / g_IocpThreadHandle / g_ServerPort right
// after it), so the same overflow clobbered the logger's critical section and
// g_ServerPort (the 24th handle, 1604, was then bound and logged as
// "Server Listen OK. Port: 1604"), which also corrupted the heap and made
// DeleteCriticalSection(&g_critCSINFO_LIST) crash on shutdown.
// Keep the original thread count and give the handle array enough storage for
// 2 * 128 logical CPUs so the original (accidental) behaviour no longer
// depends on global ordering.
#define MAX_WORKER_THREAD_STORAGE  (MAX_WORKER_THREAD * 16)

// IO Operation types
enum IO_OPERATION {
    RECV_IO = 0,
    SEND_IO = 1
};

// Per-IO Context: overlapped I/O state
typedef struct _PER_IO_CONTEXT {
    OVERLAPPED  Overlapped;                    // [0x00] Async I/O control
    WSABUF      wsabuf;                        // [0x14] Buffer descriptor
    CHAR        Buffer[MAX_BUFF_SIZE];         // [0x1C] Primary buffer
    CHAR        BufferSecond[MAX_BUFF_SIZE];   // [0x181C] Secondary buffer (queued sends)
    int         nSecondOfs;                    // [0x301C] Offset in secondary buffer
    int         nTotalBytes;                   // [0x3020] Total bytes to send
    int         nSentBytes;                    // [0x3024] Bytes already sent
    int         IOOperation;                   // [0x3028] RECV_IO or SEND_IO
    int         nWaitIO;                       // [0x302C] IO operation pending flag
} PER_IO_CONTEXT, * LPPER_IO_CONTEXT;

// Per-Socket Context: connection state
typedef struct _PER_SOCKET_CONTEXT {
    SOCKET              m_socket;              // [0x00] Socket handle
    int                 nIndex;                // [0x04] Connection index
    PER_IO_CONTEXT      IOContext[2];          // [0x08] [0]=recv, [1]=send
    volatile LONG       dwIOCount;             // [0x6068] Pending IO count
} PER_SOCKET_CONTEXT, * LPPER_SOCKET_CONTEXT;


// Public API
int  CreateGIocp(int server_port);
void DestroyGIocp();
int IoSend(int aIndex, LPBYTE lpMsg, DWORD size);
int  UpdateCompletionPort(SOCKET s, int nIndex, int bAdd);
void CloseClient(PER_SOCKET_CONTEXT* lpPerSocketContext, int bAdd);
void CloseClient(int index);
void GetLastErrorMsg();

// Internal functions
DWORD IocpServerWorker();
DWORD WINAPI IOCPWorkerThread(LPVOID lpThreadParameter);
void IOCPRecvSignal(PER_SOCKET_CONTEXT* lpPerSocketContext, PER_IO_CONTEXT* lpIOContext, DWORD dwIoSize);
void IOCPSendSignal(PER_SOCKET_CONTEXT* lpPerSocketContext, PER_IO_CONTEXT* lpIOContext, DWORD dwIoSize);
void ProcessIOCPSignal(DWORD ClientIndex, DWORD dwIoSize, PER_IO_CONTEXT* lpOverlapped);
int  ParsingRecvData(PER_IO_CONTEXT* lpIOContext, int uIndex, PER_SOCKET_CONTEXT* lpPerSocketContext);
int  IoSendSecond(PER_SOCKET_CONTEXT* lpPerSocketContext);
int  IoMoreSend(PER_SOCKET_CONTEXT* lpPerSocketContext);
void PIM_WORKERPROC(int WorkerID, int Param, void* pData);

extern SOCKET			g_Listen;
extern HANDLE			g_IocpThreadHandle;
extern CRITICAL_SECTION	g_critERROR_LOG;
extern CPIM				g_cPIM;
extern int				g_ServerPort;
extern HANDLE			g_ThreadHandles[];
extern char				ErrorMsg[];
extern DWORD			g_dwThreadCount;
extern HANDLE			g_CompletionPort;
extern LONG				g_iSockConnNum;

#endif // __GIOCP_H__