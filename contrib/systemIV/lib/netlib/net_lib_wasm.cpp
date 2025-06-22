#include "lib/universal_include.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"
#include <stdio.h>
#include <string.h>

// WebAssembly networking stubs
// These provide no-op implementations for networking functions

int GetLocalHost()
{
    // Return localhost equivalent
    return 0x7F000001; // 127.0.0.1 in network byte order
}

void GetLocalHostIP(char *str, int len)
{
    // Always return localhost for WASM
    strncpy(str, "127.0.0.1", len - 1);
    str[len - 1] = '\0';
}

void IpAddressToStr(const NetIpAddress &address, char *str)
{
    // Simple stub implementation
    sprintf(str, "127.0.0.1");
}

// NetLib class implementation
NetLib::NetLib()
{
}

NetLib::~NetLib()
{
}

bool NetLib::Initialise()
{
    // Always succeed for WASM - no actual networking to initialize
    return true;
}

#endif // TARGET_EMSCRIPTEN 