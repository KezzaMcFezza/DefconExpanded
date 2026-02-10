/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "lib/universal_include.h"

#include "SecChecksum.h"
#include "lib/tosser/directory.h"
#include <sstream>

//
// Security checksum to check integrity of recording file.
// Same implementation as Dedcon for compatibility and because
// it actually serves a great purpose.
// Its not cryptographically secure, but it doesn't need to be, 
// anyone who can decode the binary can fake recordings anyway.
// The goal is to detect accidental corruption and basic tampering.


SecChecksum::SecChecksum()
:   checksum( 4711 ),
    secretChecksum( 42 )
{
}

void SecChecksum::Add( const unsigned char *buffer, unsigned int len )
{
    //
    // Do some random nonlinear scrambling

    while( len-- > 0 )
    {
        secretChecksum *= 671;
        secretChecksum += buffer[len];
        secretChecksum ^= 0x4a626f32;

        checksum = checksum + secretChecksum * buffer[len];
    }
}

void SecChecksum::Add( const std::string &s )
{
    Add( reinterpret_cast< const unsigned char * >( s.c_str() ), (unsigned int)s.size() );
}

void SecChecksum::Add( const Directory &dir )
{
    std::ostringstream buf;
    dir.Write( buf );
    std::string data = buf.str();
    Add( data );
}

unsigned int SecChecksum::Get() const
{
    return checksum;
}
