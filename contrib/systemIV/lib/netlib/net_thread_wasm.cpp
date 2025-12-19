#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include "net_thread.h"

// WebAssembly threading stubs
// WASM doesn't support traditional threading

NetRetCode NetStartThread(NetThreadFunc functionPointer, void *arg)
{
    // Cannot start threads in WASM, return failure
    return NetNotSupported;
}

#endif // TARGET_EMSCRIPTEN 