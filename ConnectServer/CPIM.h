#ifndef __CPIM_H__
#define __CPIM_H__

#include <windows.h>
#include <vector>

struct _PER_PIM_CONTEXT
{
    SOCKET      Socket;
    unsigned int IP;
};

typedef void(__cdecl* PIM_CALLBACK)(int WorkerID, int Param, _PER_PIM_CONTEXT* Data);

struct QueueNode
{
    _PER_PIM_CONTEXT* Data;
    QueueNode* Next;
    QueueNode() : Data(NULL), Next(NULL) {}
};

class Queue
{
public:
    QueueNode* Head;
    QueueNode* Tail;
    LONG Count;

    Queue() : Head(NULL), Tail(NULL), Count(0) {}
    ~Queue()
    {
        while (Head)
        {
            QueueNode* p = Head;
            Head = Head->Next;
            delete p;
        }
    }

    int enQueue(_PER_PIM_CONTEXT* data)
    {
        QueueNode* pNode = new QueueNode();
        pNode->Data = data;
        if (Tail) Tail->Next = pNode;
        Tail = pNode;
        if (!Head) Head = Tail;
        return ++Count;
    }

    _PER_PIM_CONTEXT* deQueue()
    {
        if (!Head) return NULL;
        QueueNode* pNode = Head;
        Head = Head->Next;
        if (!Head) Tail = NULL;
        _PER_PIM_CONTEXT* Data = pNode->Data;
        delete pNode;
        --Count;
        return Data;
    }
};

struct PIM_THREAD_PARAM
{
    class CPIM* pPIM;
    int         WorkerID;
    int         Param;
};

class CPIM
{
public:
    PIM_CALLBACK            m_Callback;

    CPIM(int QueueCount);
    ~CPIM();

    bool StartWorkers(int Param);
    int  Push(int QueueIndex, _PER_PIM_CONTEXT* Data);
    static DWORD WINAPI PI_WorkerThreadProc(LPVOID lpParam);

private:
    CRITICAL_SECTION* m_CSArray;
    int                     m_QueueCount;
    std::vector<HANDLE>     m_WorkerThreads;
    Queue* m_Queues;
    HANDLE* m_Events;
};

#endif // __CPIM_H__