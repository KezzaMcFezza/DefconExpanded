#ifndef __INCLUDED_THREAD_CONTROLLER_H
#define __INCLUDED_THREAD_CONTROLLER_H

#include "lib/netlib/net_lib.h"
#include "lib/netlib/net_mutex.h"
#include "lib/netlib/condition.h"

// ThreadController is used to start and stop threads. It should be
// called only by the client thread. The life cycle of the thread is
// governed entirely by the ThreadController and the thread is
// considered to be running unless Stop() has been called.

class ThreadFunctions;

class ThreadController
{
private:
    ThreadFunctions *m_thread;


public:
    typedef void (FuncType)(ThreadFunctions *functions);

    ThreadController();
	~ThreadController();   // Will stop the thread if it is running.

    void Start( FuncType *func, void *argument = NULL ); // Will stop the already running thread before starting new function.
    bool IsRunning() const;
    void Stop();
};


// ThreadFunctions is used by the thread to allow the thread
// to gracefully shutdown when requested to stop.
class ThreadFunctions
{
private:

    ThreadController::FuncType *m_func;
    void                       *m_argument;

    // Shared data
    NetMutex                    m_mutex;
    ConditionVariable           m_cond;
    bool                        m_running;
    bool                        m_stopRequested;

    // Called on Own Thread:
    static NetCallBackRetType Start( void *threadFunctions );
    void Run();

    // Called from Client Thread:
    friend class ThreadController;
    void Stop();

public:
    ThreadFunctions( ThreadController::FuncType *func, void *argument );

    void *Argument();
    bool StopRequested();                         // Has this thread been asked to stop.
    bool Wait( double howLongMs );                // Sleep for milliseconds or until requested to stop.
                                                  // (milliseconds < 0.0 means sleep forever).
                                                  // Returns false if interrupted by a request to stop.
};



#endif // __INCLUDED_THREAD_CONTROLLER_H
