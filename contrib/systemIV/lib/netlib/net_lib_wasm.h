#ifndef INCLUDED_NET_LIB_WASM_H
#define INCLUDED_NET_LIB_WASM_H

#include <stdint.h>

#ifdef __EMSCRIPTEN_PTHREADS__
#include <pthread.h>
#include <sys/time.h>
#endif

struct wasm_sockaddr_in 
{
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    char sin_zero[8];
};

class NetUdpPacket;

typedef void * (*NetThreadFunc)(void *ptr);

#define NetGetLastError()               0
#define NetSleep(a)                     do {} while(0)
#define NetCloseSocket(a)               0
#define NetGetHostByName(a)             NULL
#define NetSetSocketNonBlocking(a)      0
#define NetSocketLenType                int
#define NetSocketHandle                 int
#define NetHostDetails                  void
#ifdef __EMSCRIPTEN_PTHREADS__
#define NetThreadHandle                 pthread_t
#define NetMutexHandle                  pthread_mutex_t
#else
#define NetThreadHandle                 int
#define NetMutexHandle                  int
#endif
#define NetCallBackRetType              void *
#define NetPollObject                   int
#define NET_SOCKET_ERROR                -1
#define NCSD_SEND                       1
#define NCSD_READ                       0
#define NetIsAddrInUse                  true
#define NetIsSocketError(a)             false
#define NetIsBlockingError(a)           false
#define NetIsConnected(a)               false
#define NetIsReset(a)                   false

#endif 