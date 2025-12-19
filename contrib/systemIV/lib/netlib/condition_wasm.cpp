#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "condition.h"
#include "net_mutex.h"

// WebAssembly ConditionVariable implementation
// WASM is single-threaded, so conditions are mostly no-ops

ConditionVariable::ConditionVariable()
{
    // No-op - WASM is single-threaded
}

ConditionVariable::~ConditionVariable()
{
    // No-op - WASM is single-threaded
}

void ConditionVariable::Wait(NetMutex *mutex, int timeoutMilliseconds)
{
    // No-op - WASM is single-threaded
    // In a real multi-threaded implementation, this would wait for a condition
    // but in WASM we just return immediately since there's no threading
}

void ConditionVariable::SignalOne()
{
    // No-op - WASM is single-threaded
}

void ConditionVariable::SignalAll()
{
    // No-op - WASM is single-threaded
}

#endif // TARGET_EMSCRIPTEN 