// ****************************************************************************
//  A platform independent mutex implementation
// ****************************************************************************

#ifndef INCLUDED_NET_MUTEX_H
#define INCLUDED_NET_MUTEX_H


#include "net_lib.h"


class NetMutex
{
public:
	NetMutex();
	~NetMutex();
	
	void			Lock();
	void			Unlock();
	
	int 			IsLocked() { return m_locked; }
	
protected:
    friend class ConditionVariable;
    friend class ConditionVariableWinXP;
    
	int 			m_locked;
	NetMutexHandle 	m_mutex;
};


class MutexLock
{
public:
	MutexLock(NetMutex *mutex): m_mutex(m_mutex) { m_mutex->Lock(); }
	~MutexLock() { m_mutex->Unlock(); }
private:
	NetMutex *m_mutex;
};


#endif
