#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "condition.h"
#include "net_mutex.h"

//
// WASM is single threaded, so conditions are no ops

ConditionVariable::ConditionVariable()
{
}

ConditionVariable::~ConditionVariable()
{
}

void ConditionVariable::Wait( NetMutex *mutex, int timeoutMilliseconds )
{
}

void ConditionVariable::SignalOne()
{
}

void ConditionVariable::SignalAll()
{
}

#endif // TARGET_EMSCRIPTEN