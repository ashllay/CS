#include "StdAfx.h"
#include "CPIM.h"

CPIM::CPIM(int QueueCount)
{
    m_QueueCount = QueueCount;
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

    delete[] m_CSArray;
    delete[] m_Queues;
    delete[] m_Events;
}

bool CPIM::StartWorkers(int Param)
{
    for (int i = 0; i < m_QueueCount; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            PIM_THREAD_PARAM* pInfo = new PIM_THREAD_PARAM();
            pInfo->pPIM = this;
            pInfo->WorkerID = i;
            pInfo->Param = Param;

            DWORD dwThreadID = 0;
            HANDLE hThread = CreateThread(NULL, 0, PI_WorkerThreadProc, pInfo, 0, &dwThreadID);
            if (!hThread)
            {
                delete pInfo;
                return false;
            }
            m_WorkerThreads.push_back(hThread);
        }
    }
    return true;
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

DWORD WINAPI CPIM::PI_WorkerThreadProc(LPVOID lpParam)
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
            if (Remaining > 0)
                SetEvent(pPIM->m_Events[WorkerID]);

            if (pPIM->m_Callback)
                pPIM->m_Callback(WorkerID, Param, Data);

            delete Data;
        }
    }
    return 0;
}