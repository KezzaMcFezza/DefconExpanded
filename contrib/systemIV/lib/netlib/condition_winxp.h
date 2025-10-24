#ifndef _included_condition_winxp_h
#define _included_condition_winxp_h

#include "lib/netlib/net_mutex.h"

#include <queue>

/*
 * Condition Variable Class for Windows XP.
 *
 */

class ConditionVariableWinXP
{
private:

    NetMutex m_mutex;
    std::queue<HANDLE> m_waiting;
    std::queue<HANDLE> m_reusable;

public:
    ConditionVariableWinXP();
    ~ConditionVariableWinXP();

    //  Grabs the mutex and waits for the variable to be signaled
    //  before releasing the mutex.
    void Wait( NetMutex *mutex, int timeoutMilliseconds /* -1 forever */ );

    // Wakes up one thread waiting on the condition variable.
    void SignalOne();

    // wakes up all threads waiting on the condition variable.
    void SignalAll();

};


#endif // _included_condition_winxp_h
