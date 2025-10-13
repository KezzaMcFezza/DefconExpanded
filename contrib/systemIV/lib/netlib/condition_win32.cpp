#include "lib/universal_include.h"

#include "condition.h"

#include "lib/debug_utils.h"
#include "lib/gucci/win32library.h"
#include "net_mutex.h"
#include "condition_winxp.h"

static bool s_initialised = false;
static bool s_usingConditionVariables = false;

static Win32Library s_library("kernel32.dll");

typedef VOID WINAPI InitializeConditionVariableFunc(PCONDITION_VARIABLE);
typedef BOOL WINAPI SleepConditionVariableCSFunc(PCONDITION_VARIABLE, PCRITICAL_SECTION, DWORD);
typedef VOID WINAPI WakeConditionVariableFunc(PCONDITION_VARIABLE);
typedef VOID WINAPI WakeAllConditionVariableFunc(PCONDITION_VARIABLE ConditionVariable);

static InitializeConditionVariableFunc *s_initialiseConditionVariableFunc = NULL;
static SleepConditionVariableCSFunc *s_sleepConditionVariableCSFunc = NULL;
static WakeConditionVariableFunc *s_wakeConditionVariableFunc = NULL;
static WakeAllConditionVariableFunc *s_wakeAllConditionVariableFunc = NULL;


void Initialise()
{
    if( s_initialised ) return;

    s_initialised = true;

    s_initialiseConditionVariableFunc = (InitializeConditionVariableFunc *) s_library.GetProcAddress("InitializeConditionVariable");
    s_sleepConditionVariableCSFunc = (SleepConditionVariableCSFunc *) s_library.GetProcAddress("SleepConditionVariableCS");
    s_wakeConditionVariableFunc = (WakeConditionVariableFunc *) s_library.GetProcAddress("WakeConditionVariable");
    s_wakeAllConditionVariableFunc = (WakeAllConditionVariableFunc *) s_library.GetProcAddress("WakeAllConditionVariable" );

    if( s_initialiseConditionVariableFunc )
    {
        AppDebugOut( "Using Native Win32 Condition Variables\n" );
    }
    else
    {
        AppDebugOut( "Using Windows XP Condition Variables\n" );
    }
}



ConditionVariable::ConditionVariable()
{
    if( !s_initialised ) Initialise();

    if( s_initialiseConditionVariableFunc )
    {
        s_initialiseConditionVariableFunc( &m_var );
        m_windowsXP = NULL;
    }
    else
    {
        m_windowsXP = new ConditionVariableWinXP;
    }
}


ConditionVariable::~ConditionVariable()
{
    if( m_windowsXP )
    {
        delete m_windowsXP;
        m_windowsXP = NULL;
    }
}


void ConditionVariable::Wait( NetMutex *mutex, int timeoutMilliseconds /* -1 forever */ )
{
    if( m_windowsXP )
    {
        m_windowsXP->Wait( mutex, timeoutMilliseconds );
    }
    else
    {
        if( timeoutMilliseconds < 0 ) timeoutMilliseconds = INFINITE;
        s_sleepConditionVariableCSFunc( &m_var, &mutex->m_mutex, timeoutMilliseconds );
    }
}


void ConditionVariable::SignalOne()
{
    if( m_windowsXP )
    {
        m_windowsXP->SignalOne();
    }
    else
    {
        s_wakeConditionVariableFunc( &m_var );
    }
}


void ConditionVariable::SignalAll()
{
    if( m_windowsXP )
    {
        m_windowsXP->SignalAll();
    }
    else
    {
        s_wakeAllConditionVariableFunc( &m_var );
    }
}

