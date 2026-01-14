#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_mutex.h"

//
// WASM is single threaded, so conditions are no ops

NetMutex::NetMutex()
{
	m_mutex = 0;
}

NetMutex::~NetMutex()
{
}

void NetMutex::Lock()
{
}

void NetMutex::Unlock()
{
}

#endif // TARGET_EMSCRIPTEN