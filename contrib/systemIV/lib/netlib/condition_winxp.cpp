#include "systemiv.h"

#include "condition_winxp.h"

#include "lib/debug/debug_utils.h"


ConditionVariableWinXP::ConditionVariableWinXP()
{
}


ConditionVariableWinXP::~ConditionVariableWinXP()
{
    AppAssert( m_waiting.empty() );

    while( !m_reusable.empty() )
    {
        HANDLE event = m_reusable.front();
        CloseHandle( event );
        m_reusable.pop();
    }
}


void ConditionVariableWinXP::Wait( NetMutex *mutex, int timeoutMilliseconds /* -1 forever */ )
{
    HANDLE event;

    {
        MutexLock lock( &m_mutex );

        if( !m_reusable.empty() )
        {
            event = m_reusable.front();
            m_reusable.pop();
        }
        else
        {
            event = CreateEvent( NULL, FALSE, FALSE, NULL );
        }

        m_waiting.push( event );
    }

    mutex->Unlock();

    DWORD result = WaitForSingleObject( event, timeoutMilliseconds );

    {
        MutexLock lock( &m_mutex );

        if( result == WAIT_TIMEOUT )
        {
            // The wait timed out without signalling the event. We need to
            // remove the event from the queue.

            std::queue<HANDLE> filtered;

            while( !m_waiting.empty() )
            {
                HANDLE waiting = m_waiting.front();
                if( waiting != event ) filtered.push( waiting );
            }

            std::swap( m_waiting, filtered );
        }

        m_reusable.push( event );
    }

    mutex->Lock();
}


void ConditionVariableWinXP::SignalOne()
{
    MutexLock lock( &m_mutex );

    if( m_waiting.empty() ) return;

    HANDLE event = m_waiting.front();
    m_waiting.pop();

    SetEvent( event );
}


void ConditionVariableWinXP::SignalAll()
{
    MutexLock lock( &m_mutex );

    while( !m_waiting.empty() )
    {
        HANDLE event = m_waiting.front();
        m_waiting.pop();

        SetEvent( event );
    }
}



