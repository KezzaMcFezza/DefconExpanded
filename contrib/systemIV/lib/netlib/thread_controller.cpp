#include "systemiv.h"

#include "thread_controller.h"

#include "lib/hi_res_time.h"


// ================================================================================

ThreadController::ThreadController()
	: m_thread( NULL )
{
}


ThreadController::~ThreadController()
{
	Stop();
}


void ThreadController::Start( FuncType *func, void *argument )
{
	Stop();
	m_thread = new ThreadFunctions( func, argument );
	NetStartThread( &ThreadFunctions::Start, m_thread );
}


bool ThreadController::IsRunning() const
{
	// Note, we do not care if the thread has actually exited by itself.
	// This method returns as follows:
	// false...  Start()  true...   Stop()  false...

	return m_thread != NULL;
}


void ThreadController::Stop()
{
	if ( !m_thread )
		return;

	m_thread->Stop();
	delete m_thread;
	m_thread = NULL;
}


// ================================================================================


NetCallBackRetType ThreadFunctions::Start( void *threadFunctions )
{
	( (ThreadFunctions *)threadFunctions )->Run();
	return 0;
}


void ThreadFunctions::Run()
{
	( *m_func )( this );

	m_mutex.Lock();
	m_running = false;
	m_cond.SignalOne();
	m_mutex.Unlock();
}


void ThreadFunctions::Stop()
{
	// This method is called from the client thread.
	m_mutex.Lock();
	m_stopRequested = true;
	m_cond.SignalOne();

	while ( m_running )
	{
		m_cond.Wait( &m_mutex, -1 /* Forever */ );
	}
	m_mutex.Unlock();
}


ThreadFunctions::ThreadFunctions( ThreadController::FuncType *func, void *argument )
	: m_func( func ),
	  m_argument( argument ),
	  m_running( true ),
	  m_stopRequested( false )
{
}


void *ThreadFunctions::Argument()
{
	return m_argument;
}


bool ThreadFunctions::StopRequested()
{
	bool stopRequested = false;
	m_mutex.Lock();
	stopRequested = m_stopRequested;
	m_mutex.Unlock();
	return stopRequested;
}


bool ThreadFunctions::Wait( double howLongMs )
{
	double startTime = GetHighResTime();

	m_mutex.Lock();
	while ( !m_stopRequested )
	{
		if ( howLongMs < 0.0 /* Sleep forever, until stop requested */ )
		{
			m_cond.Wait( &m_mutex, -1 /* Forever */ );
			continue;
		}

		double elapsedMs = 1000.0 * ( GetHighResTime() - startTime );

		// Return true if we have completed our wait.
		if ( elapsedMs >= howLongMs )
		{
			m_mutex.Unlock();
			return true;
		}

		m_cond.Wait( &m_mutex, howLongMs - elapsedMs );
	}
	m_mutex.Unlock();

	return false;
}
