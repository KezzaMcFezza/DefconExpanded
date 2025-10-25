#include "lib/universal_include.h"
#include <cstring>

#include "unrar/rartypes.hpp"
#include "unrar/sha1.hpp"


void HashData( char *_data, int _hashToken, char *_result )
{
	sha1_context c;
	sha1_init(&c);

	char fullString[512];
	sprintf( fullString, "%s-%d", _data, _hashToken );

	sha1_process(&c, (byte *) fullString, (size_t) strlen(fullString));

    uint32 hash[5];
	sha1_done(&c, hash);
	
    sprintf(_result, "hsh%04x%04x%04x%04x%04x", hash[0], hash[1], hash[2], hash[3], hash[4]);
}


void HashData( char *_data, char *_result )
{
    sha1_context c;
    sha1_init(&c);

    sha1_process(&c, (byte *) _data, (size_t) strlen(_data));

    uint32 hash[5];
    sha1_done(&c, hash);

    sprintf(_result, "hsh%04x%04x%04x%04x%04x", hash[0], hash[1], hash[2], hash[3], hash[4]);
}
