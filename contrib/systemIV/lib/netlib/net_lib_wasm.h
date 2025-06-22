#ifndef INCLUDED_NET_LIB_WASM_H
#define INCLUDED_NET_LIB_WASM_H

// WebAssembly networking stubs
// WASM doesn't support traditional BSD sockets, so we provide stub implementations

#include <stdint.h>

// Forward declare a minimal sockaddr_in equivalent for WASM
struct wasm_sockaddr_in {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char sin_zero[8];
};

class NetUdpPacket;

// Stub function pointer type
typedef void * (*NetThreadFunc)(void *ptr);

// Define stub implementations for WASM
#define NetGetLastError()               0
#define NetSleep(a)                     do {} while(0)  // No-op
#define NetCloseSocket(a)               0
#define NetGetHostByName(a)             NULL
#define NetSetSocketNonBlocking(a)      0

// Define stub types for WASM
#define NetSocketLenType                int
#define NetSocketHandle                 int
#define NetHostDetails                  void
#define NetThreadHandle                 int
#define NetCallBackRetType              void *
#define NetPollObject                   int
#define NetMutexHandle                  int

// Define stub constants for WASM
#define NET_SOCKET_ERROR                -1
#define NCSD_SEND                       1
#define NCSD_READ                       0

// Define stub condition tests for WASM
#define NetIsAddrInUse                  true
#define NetIsSocketError(a)             false
#define NetIsBlockingError(a)           false
#define NetIsConnected(a)               false
#define NetIsReset(a)                   false

#endif 