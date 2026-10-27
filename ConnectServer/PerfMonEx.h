#ifndef __PERFMONEX_H__
#define __PERFMONEX_H__

#pragma once

#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>

#include <map>
#include <vector>
using namespace std;
#pragma comment(lib, "pdh.lib")

#define MAX_RAW_VALUES 20

// ----------------------------------------------------------------------
// Counter Names
// ----------------------------------------------------------------------

#define CNTR_CPU                   "\\Processor(_Total)\\% Processor Time"

#define CNTR_MEMINUSE_BYTES        "\\Memory\\Committed Bytes"
#define CNTR_MEMAVAIL_BYTES        "\\Memory\\Available Bytes"
#define CNTR_MEMAVAIL_KB           "\\Memory\\Available KBytes"
#define CNTR_MEMAVAIL_MB           "\\Memory\\Available MBytes"

#define CNTR_MEMINUSE_PERCENT      "\\Memory\\% Committed Bytes In Use"
#define CNTR_MEMLIMIT_BYTES        "\\Memory\\Commit Limit"

// ----------------------------------------------------------------------

typedef struct _PDHCOUNTERSTRUCT
{
	int             nIndex;
	LONG            lValue;
	HCOUNTER        hCounter;
	int             nNextIndex;
	int             nOldestIndex;
	int             nRawCount;

	PDH_RAW_COUNTER RawValue[MAX_RAW_VALUES];

} PDHCOUNTERSTRUCT, * LPPDHCOUNTERSTRUCT;

// ----------------------------------------------------------------------

class CPerfMonEx
{
public:

	CPerfMonEx();
	virtual ~CPerfMonEx();

public:

	BOOL Initialize();
	void Uninitialize();

public:

	int AddCounter(const char* pszCounterName);
	BOOL RemoveCounter(int nIndex);

public:

	BOOL CollectQueryData();
	BOOL GetStatistics(long* nMin, long* nMax, long* nMean, int nIndex);
	long GetCounterValue(int nIndex);

protected:
	LPPDHCOUNTERSTRUCT GetCounterStruct(int nIndex);

	BOOL UpdateValue(PDHCOUNTERSTRUCT& Counter);
	BOOL UpdateRawValue(PDHCOUNTERSTRUCT& Counter);

protected:

	HQUERY m_hQuery;
	vector<LPPDHCOUNTERSTRUCT> m_aCounters;	//std::map<int, PDHCOUNTERSTRUCT> m_aCounters;
	int m_nNextIndex;
};

#endif