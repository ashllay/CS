#include "StdAfx.h"
#include <process.h>
#include "CPIM.h"

CPIM::CPIM(int QueueCount)
{
    m_QueueCount = QueueCount;
    m_QueueIndex = 0;
    m_Callback = NULL;

    m_CSArray = new CRITICAL_SECTION[QueueCount];
    m_Queues = new Queue[QueueCount];
    m_Events = new HANDLE[QueueCount];

    for (int i = 0; i < QueueCount; i++)
    {
        InitializeCriticalSection(&m_CSArray[i]);
        m_Events[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
}

CPIM::~CPIM()
{
    for (size_t i = 0; i < m_WorkerThreads.size(); i++)
    {
        TerminateThread(m_WorkerThreads[i], 0);
        CloseHandle(m_WorkerThreads[i]);
    }
    m_WorkerThreads.clear();

    for (int i = 0; i < m_QueueCount; i++)
    {
        DeleteCriticalSection(&m_CSArray[i]);
        CloseHandle(m_Events[i]);
    }

    // NOTE: The original destructor (0x402D80) does not free m_CSArray,
    // m_Queues or m_Events; only the worker-handle vector storage is released.
    // These allocations are intentionally leaked here as well.
}

void CPIM::StartWorkers()
{
    for (int i = 0; i < m_QueueCount; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            PIM_THREAD_PARAM* pInfo = new PIM_THREAD_PARAM();
            pInfo->pPIM = this;
            pInfo->Param = j;
            pInfo->WorkerID = i;

            unsigned int dwThreadID = 0;
            // Original uses _beginthreadex and pushes the handle into the
            // worker vector without checking for failure (0x4012E2).
            HANDLE hThread = (HANDLE)_beginthreadex(NULL, 0, PI_WorkerThreadProc, pInfo, 0, &dwThreadID);
            m_WorkerThreads.push_back(hThread);

            // The original sleeps 100 ms after every worker is created.
            Sleep(100);
        }
    }
}

int CPIM::Push(int QueueIndex, _PER_PIM_CONTEXT* Data)
{
    if (QueueIndex < 0 || QueueIndex >= m_QueueCount)
        return 0;

    EnterCriticalSection(&m_CSArray[QueueIndex]);
    int Count = m_Queues[QueueIndex].enQueue(Data);
    LeaveCriticalSection(&m_CSArray[QueueIndex]);
    SetEvent(m_Events[QueueIndex]);
    return Count;
}

unsigned int __stdcall CPIM::PI_WorkerThreadProc(void* lpParam)
{
    PIM_THREAD_PARAM* p = (PIM_THREAD_PARAM*)lpParam;
    CPIM* pPIM = p->pPIM;
    int WorkerID = p->WorkerID;
    int Param = p->Param;
    delete p;

    for (;;)
    {
        WaitForSingleObject(pPIM->m_Events[WorkerID], INFINITE);
        EnterCriticalSection(&pPIM->m_CSArray[WorkerID]);
        _PER_PIM_CONTEXT* Data = pPIM->m_Queues[WorkerID].deQueue();
        LONG Remaining = pPIM->m_Queues[WorkerID].Count;
        LeaveCriticalSection(&pPIM->m_CSArray[WorkerID]);

        if (Data)
        {
            if (Remaining != 0)
                SetEvent(pPIM->m_Events[WorkerID]);

            if (pPIM->m_Callback)
                pPIM->m_Callback(WorkerID, Param, Data);

            delete Data;
        }
    }
    return 0;
}