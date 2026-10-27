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
	if (PdhOpenQuery(NULL, 1, &this->m_hQuery) != ERROR_SUCCESS)
	{
		return FALSE;
	}
	m_aCounters.reserve(10);
	return TRUE;
}

// ----------------------------------------------------------------------
// Uninitialize
// ----------------------------------------------------------------------

void CPerfMonEx::Uninitialize()
{
	//if (this->m_hQuery != NULL)
	//{
	//	PdhCloseQuery(this->m_hQuery);

	//	this->m_hQuery = NULL;
	//}

	//this->m_aCounters.clear();
	PdhCloseQuery(&m_hQuery);

	for (int i = 0; i < m_aCounters.size(); i++)
	{
		delete m_aCounters.at(i);
	}

	m_aCounters.clear();
}

// ----------------------------------------------------------------------
// AddCounter
// ----------------------------------------------------------------------

int CPerfMonEx::AddCounter(const char* pszCounterName)
{
	//PDHCOUNTERSTRUCT Counter;
	//memset(&Counter, 0, sizeof(Counter));
	//PDH_STATUS Status;

	//Status = PdhAddCounterA(this->m_hQuery, pszCounterName, 0, &Counter.hCounter);
	//if (Status != ERROR_SUCCESS)
	//{
	//	return -1;
	//}

	//Counter.nIndex = this->m_nNextIndex;
	//this->m_aCounters[this->m_nNextIndex] = Counter;

	//return this->m_nNextIndex++;
	LPPDHCOUNTERSTRUCT pCounter = new PDHCOUNTERSTRUCT;
	if (!pCounter) return -1;

	memset(pCounter, 0, sizeof(PDHCOUNTERSTRUCT));

	// add to current query
	if (PdhAddCounter(m_hQuery, pszCounterName, (DWORD)pCounter, &(pCounter->hCounter)) != ERROR_SUCCESS)
	{
		delete pCounter; // clean mem
		return -1;
	}

	// insert counter into array(s)
	pCounter->nIndex = m_nNextIndex++;
	pCounter->lValue = 0;
	pCounter->nNextIndex = 0;
	pCounter->nOldestIndex = 0;
	pCounter->nRawCount = 0;
	m_aCounters.push_back(pCounter);

	return pCounter->nIndex;
}

// ----------------------------------------------------------------------
// RemoveCounter
// ----------------------------------------------------------------------

BOOL CPerfMonEx::RemoveCounter(int nIndex)
{
	//std::map<int, PDHCOUNTERSTRUCT>::iterator it;

	//it = this->m_aCounters.find(nIndex);

	//if (it == this->m_aCounters.end())
	//{
	//	return FALSE;
	//}

	//if (PdhRemoveCounter(it->second.hCounter) != ERROR_SUCCESS)
	//{
	//	return FALSE;
	//}

	//this->m_aCounters.erase(it);

	//return TRUE;
	LPPDHCOUNTERSTRUCT pCounter = GetCounterStruct(nIndex);
	if (!pCounter) return false;

	if (PdhRemoveCounter(pCounter->hCounter) != ERROR_SUCCESS)
		return false;

	return true;
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
	//PDH_FMT_COUNTERVALUE Value;

	//if (PdhGetFormattedCounterValue(Counter.hCounter, PDH_FMT_LONG, NULL, &Value) != ERROR_SUCCESS)
	//{
	//	return FALSE;
	//}

	//if (Value.CStatus != ERROR_SUCCESS)
	//{
	//	return FALSE;
	//}

	//Counter.lValue = Value.longValue;

	//return TRUE;
	PDH_FMT_COUNTERVALUE pdhFormattedValue;

	// get the value from the PDH
	if (PdhGetFormattedCounterValue(Counter.hCounter, PDH_FMT_LONG, NULL, &pdhFormattedValue) != ERROR_SUCCESS)
		return false;

	// test the value for validity
	if (pdhFormattedValue.CStatus != ERROR_SUCCESS)
		return false;

	// set value
	Counter.lValue = pdhFormattedValue.longValue;

	return true;
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
// GetStatistics
// ----------------------------------------------------------------------

BOOL CPerfMonEx::GetStatistics(long* nMin, long* nMax, long* nMean, int nIndex)
{
	//std::map<int, PDHCOUNTERSTRUCT>::iterator it;
	//it = this->m_aCounters.find(nIndex);

	//if (it == this->m_aCounters.end())
	//{
	//	return FALSE;
	//}

	//PDHCOUNTERSTRUCT& Counter = it->second;

	//PDH_STATISTICS Stats;

	//if (PdhComputeCounterStatistics(
	//	Counter.hCounter, PDH_FMT_LONG, Counter.nOldestIndex,
	//	Counter.nRawCount, Counter.RawValue, &Stats) != ERROR_SUCCESS)
	//{
	//	return FALSE;
	//}

	//*nMin = (Stats.min.CStatus == ERROR_SUCCESS) ? Stats.min.longValue : 0;
	//*nMax = (Stats.max.CStatus == ERROR_SUCCESS) ? Stats.max.longValue : 0;
	//*nMean = (Stats.mean.CStatus == ERROR_SUCCESS) ? Stats.mean.longValue : 0;

	//return TRUE;
	PDH_STATISTICS pdhStats;
	LPPDHCOUNTERSTRUCT pCounter = GetCounterStruct(nIndex);
	if (!pCounter) return false;

	if (PdhComputeCounterStatistics(pCounter->hCounter, PDH_FMT_LONG, pCounter->nOldestIndex, pCounter->nRawCount, pCounter->RawValue, &pdhStats) != ERROR_SUCCESS)
		return false;

	// set values
	if (pdhStats.min.CStatus != ERROR_SUCCESS)
		*nMin = 0;
	else
		*nMin = pdhStats.min.longValue;

	if (pdhStats.max.CStatus != ERROR_SUCCESS)
		*nMax = 0;
	else
		*nMax = pdhStats.max.longValue;

	if (pdhStats.mean.CStatus != ERROR_SUCCESS)
		*nMean = 0;
	else
		*nMean = pdhStats.mean.longValue;

	return true;
}

// ----------------------------------------------------------------------
// GetCounterValue
// ----------------------------------------------------------------------

long CPerfMonEx::GetCounterValue(int nIndex)
{
	//std::map<int, PDHCOUNTERSTRUCT>::iterator it;

	//it = this->m_aCounters.find(nIndex);

	//if (it == this->m_aCounters.end())
	//{
	//	return -999L;
	//}

	//PDHCOUNTERSTRUCT& Counter = it->second;

	//if (this->UpdateValue(Counter) == FALSE)
	//{
	//	return -999L;
	//}

	//if (this->UpdateRawValue(Counter) == FALSE)
	//{
	//	return -999L;
	//}

	//return Counter.lValue;
	LPPDHCOUNTERSTRUCT pCounter = GetCounterStruct(nIndex);
	if (!pCounter) return -999L;

	// update the value(s)
	if (!UpdateValue(*pCounter)) return -999L;
	if (!UpdateRawValue(*pCounter)) return -999L;

	// return the value
	return pCounter->lValue;
}

LPPDHCOUNTERSTRUCT CPerfMonEx::GetCounterStruct(int nIndex)
{
	for (int i = 0; i < m_aCounters.size(); i++)
	{
		if (m_aCounters.at(i)->nIndex == nIndex)
			return m_aCounters.at(i);
	}

	return NULL;
}
