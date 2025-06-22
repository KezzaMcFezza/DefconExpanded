#include "lib/universal_include.h"

#include "condition.h"
#include "net_mutex.h"

#include <sys/time.h>

ConditionVariable::ConditionVariable()
{
    pthread_cond_init( &m_var, NULL );
}


ConditionVariable::~ConditionVariable()
{
    pthread_cond_destroy( &m_var );
}


void ConditionVariable::Wait( NetMutex *mutex, int timeoutMilliseconds /* -1 forever */ )
{
    if( timeoutMilliseconds < 0 )
    {
        pthread_cond_wait( &m_var, &mutex->m_mutex );
    }
    else
    {
        struct timespec abstime;
        struct timeval  now;

        gettimeofday(&now, NULL);

        abstime.tv_sec  = now.tv_sec;
        abstime.tv_nsec = now.tv_usec * 1000L;

        abstime.tv_sec += timeoutMilliseconds / 1000;
        abstime.tv_nsec += 1000000L * (timeoutMilliseconds % 1000);

        if( abstime.tv_nsec >= 1000000000L )
        {
            abstime.tv_nsec -= 1000000000L;
            abstime.tv_sec ++;
        }

        pthread_cond_timedwait( &m_var, &mutex->m_mutex, &abstime );
    }
}


// Wakes up one thread waiting on the condition variable.
void ConditionVariable::SignalOne()
{
    pthread_cond_signal( &m_var );
}


// wakes up all threads waiting on the condition variable.
void ConditionVariable::SignalAll()
{
    pthread_cond_broadcast( &m_var );
}
