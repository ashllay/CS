// CriticalSec.h: interface for the CCriticalSec class.
//
//////////////////////////////////////////////////////////////////////

#ifndef __CRITICALSEC_H__
#define __CRITICALSEC_H__

class CCriticalSec  
{
	CRITICAL_SECTION	sec;

public:

	CCriticalSec();
	~CCriticalSec();

	inline void Lock(){ EnterCriticalSection(&sec); }
	inline void UnLock(){ LeaveCriticalSection(&sec); }
};

class CCSLock  
{
	CCriticalSec*	m_pCriticalSec;
public:
	
	CCSLock(CCriticalSec* pCriticalSec);
	~CCSLock();		
};
extern CCriticalSec	m_OBJLock;
#endif // !defined(AFX_CRITICALSEC_H__16786BC1_2201_11D5_BBA8_00A02490CE8E__INCLUDED_)
