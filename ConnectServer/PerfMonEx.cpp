// PerfMonEx.cpp
#include "StdAfx.h"
#include "PerfMonEx.h"

// ----------------------------------------------------------------------
// Constructor
// ----------------------------------------------------------------------

CPerfMonEx::CPerfMonEx()
{
	this->m_hQuery = NULL;

	this->m_nNextIndex = 0;
}

// ----------------------------------------------------------------------
// Destructor
// ----------------------------------------------------------------------

CPerfMonEx::~CPerfMonEx()
{
	this->Uninitialize();
}

// ----------------------------------------------------------------------
// Initialize
// ----------------------------------------------------------------------

BOOL CPerfMonEx::Initialize()
{
	if (PdhOpenQuery(NULL, 0, &this->m_hQuery) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	return TRUE;
}

// ----------------------------------------------------------------------
// Uninitialize
// ----------------------------------------------------------------------

void CPerfMonEx::Uninitialize()
{
	if (this->m_hQuery != NULL)
	{
		PdhCloseQuery(this->m_hQuery);

		this->m_hQuery = NULL;
	}

	this->m_aCounters.clear();
}

// ----------------------------------------------------------------------
// AddCounter
// ----------------------------------------------------------------------

int CPerfMonEx::AddCounter(const char* pszCounterName)
{
	PDHCOUNTERSTRUCT Counter;
	memset(&Counter, 0, sizeof(Counter));
	PDH_STATUS Status;

	Status = PdhAddCounterA(this->m_hQuery, pszCounterName, 0, &Counter.hCounter);
	if (Status != ERROR_SUCCESS)
	{
		return -1;
	}

	Counter.nIndex = this->m_nNextIndex;
	this->m_aCounters[this->m_nNextIndex] = Counter;

	return this->m_nNextIndex++;
}

// ----------------------------------------------------------------------
// RemoveCounter
// ----------------------------------------------------------------------

BOOL CPerfMonEx::RemoveCounter(int nIndex)
{
	std::map<int, PDHCOUNTERSTRUCT>::iterator it;

	it = this->m_aCounters.find(nIndex);

	if (it == this->m_aCounters.end())
	{
		return FALSE;
	}

	if (PdhRemoveCounter(it->second.hCounter) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	this->m_aCounters.erase(it);

	return TRUE;
}

// ----------------------------------------------------------------------
// CollectQueryData
// ----------------------------------------------------------------------

BOOL CPerfMonEx::CollectQueryData()
{
	if (PdhCollectQueryData(this->m_hQuery) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	return TRUE;
}

// ----------------------------------------------------------------------
// UpdateValue
// ----------------------------------------------------------------------

BOOL CPerfMonEx::UpdateValue(PDHCOUNTERSTRUCT& Counter)
{
	PDH_FMT_COUNTERVALUE Value;

	if (PdhGetFormattedCounterValue(Counter.hCounter, PDH_FMT_LONG, NULL, &Value) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	if (Value.CStatus != ERROR_SUCCESS)
	{
		return FALSE;
	}

	Counter.lValue = Value.longValue;

	return TRUE;
}

// ----------------------------------------------------------------------
// UpdateRawValue
// ----------------------------------------------------------------------

BOOL CPerfMonEx::UpdateRawValue(PDHCOUNTERSTRUCT& Counter)
{
	PPDH_RAW_COUNTER lpRaw;

	lpRaw = &Counter.RawValue[Counter.nNextIndex];

	if (PdhGetRawCounterValue(Counter.hCounter, NULL, lpRaw) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	Counter.nRawCount = min(Counter.nRawCount + 1, MAX_RAW_VALUES);
	Counter.nNextIndex = (Counter.nNextIndex + 1) % MAX_RAW_VALUES;

	if (Counter.nRawCount >= MAX_RAW_VALUES)
	{
		Counter.nOldestIndex = Counter.nNextIndex;
	}

	return TRUE;
}

// ----------------------------------------------------------------------
// GetCounterValue
// ----------------------------------------------------------------------

long CPerfMonEx::GetCounterValue(int nIndex)
{
	std::map<int, PDHCOUNTERSTRUCT>::iterator it;

	it = this->m_aCounters.find(nIndex);

	if (it == this->m_aCounters.end())
	{
		return -999L;
	}

	PDHCOUNTERSTRUCT& Counter = it->second;

	if (this->UpdateValue(Counter) == FALSE)
	{
		return -999L;
	}

	if (this->UpdateRawValue(Counter) == FALSE)
	{
		return -999L;
	}

	return Counter.lValue;
}

// ----------------------------------------------------------------------
// GetStatistics
// ----------------------------------------------------------------------

BOOL CPerfMonEx::GetStatistics(long* nMin, long* nMax, long* nMean, int nIndex)
{
	std::map<int, PDHCOUNTERSTRUCT>::iterator it;
	it = this->m_aCounters.find(nIndex);

	if (it == this->m_aCounters.end())
	{
		return FALSE;
	}

	PDHCOUNTERSTRUCT& Counter = it->second;

	PDH_STATISTICS Stats;

	if (PdhComputeCounterStatistics(
		Counter.hCounter, PDH_FMT_LONG, Counter.nOldestIndex, 
		Counter.nRawCount, Counter.RawValue, &Stats) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	*nMin = (Stats.min.CStatus == ERROR_SUCCESS) ? Stats.min.longValue : 0;
	*nMax = (Stats.max.CStatus == ERROR_SUCCESS) ? Stats.max.longValue : 0;
	*nMean = (Stats.mean.CStatus == ERROR_SUCCESS) ? Stats.mean.longValue : 0;

	return TRUE;
}