#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_thread.h"

//
// WASM is single threaded, so conditions are no ops

NetRetCode NetStartThread( NetThreadFunc functionPointer, void *arg )
{
	return NetNotSupported;
}

#endif // TARGET_EMSCRIPTEN