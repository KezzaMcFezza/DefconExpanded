/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef INCLUDED_SECCHECKSUM_H
#define INCLUDED_SECCHECKSUM_H

#include <string>

class Directory;

class SecChecksum
{
public:
    SecChecksum();

    void Add( const unsigned char *buffer, unsigned int len );
    void Add( const std::string &s );
    void Add( const Directory &dir );

    unsigned int Get() const;

private:
    unsigned int checksum;
    unsigned int secretChecksum;
};

#endif
