/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_FileTagReader_INCLUDED
#define DEDCON_FileTagReader_INCLUDED

#include "TagReader.h"
#include "CompressedTag.h"
#include <iostream>

//! reads a tag from a file
class FileTagReader: public CompressedTag, public TagReader
{
public:
    FileTagReader( std::istream & s, unsigned int maxSize = 0 )
    : CompressedTag( s, maxSize ), TagReader( Buffer(), Size() )
    {
        if ( s.eof() )
        {
            Obsolete();
        }
    }
};

#endif // DEDCON_FileTagReader_INCLUDED
