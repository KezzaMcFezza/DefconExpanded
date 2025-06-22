#ifndef _included_condition_h
#define _included_condition_h

class ConditionVariableWinXP;
class NetMutex;

#ifndef WIN32
#include <pthread.h>
#endif

/*
 * Condition Variable Class.
 *
 * Linux and Mac OS X offer only condition variables.
 * Windows after Windows XP offers condition variables.
 * Windows XP and before offer Events which can be used to
 * implement condition variables;
 */

class ConditionVariable
{
#ifdef WIN32
    // Direct support after Windows XP
    CONDITION_VARIABLE      m_var;

    // Windows XP and before.
    ConditionVariableWinXP *m_windowsXP;
#else
    pthread_cond_t     m_var;
#endif

public:
    ConditionVariable();
    ~ConditionVariable();

    //  Grabs the mutex and waits for the variable to be signaled
    //  before releasing the mutex.
    void Wait( NetMutex *mutex, int timeoutMilliseconds /* -1 forever */ );

    // Wakes up one thread waiting on the condition variable.
    void SignalOne();

    // wakes up all threads waiting on the condition variable.
    void SignalAll();
};


#endif // _included_condition_h
