#include "lib/universal_include.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_mutex.h"

// WebAssembly mutex stubs
// WASM is single-threaded, so mutexes are no-ops

NetMutex::NetMutex()
{
    m_mutex = 0; // Dummy value
}

NetMutex::~NetMutex()
{
    // No-op
}

void NetMutex::Lock()
{
    // No-op - WASM is single-threaded
}

void NetMutex::Unlock()
{
    // No-op - WASM is single-threaded
}

#endif // TARGET_EMSCRIPTEN 