#include "systemiv.h"

#ifdef TARGET_EMSCRIPTEN

#include "net_lib.h"

//
// WASM is single threaded, so conditions are no ops

int GetLocalHost()
{
	return 0x7F000001; // 127.0.0.1 in network byte order
}


void GetLocalHostIP( char *str, int len )
{
	strncpy( str, "127.0.0.1", len - 1 );
	str[len - 1] = '\0';
}


void IpAddressToStr( const NetIpAddress &address, char *str )
{
	sprintf( str, "127.0.0.1" );
}


NetLib::NetLib()
{
}


NetLib::~NetLib()
{
}


bool NetLib::Initialise()
{
	return true;
}

#endif // TARGET_EMSCRIPTEN