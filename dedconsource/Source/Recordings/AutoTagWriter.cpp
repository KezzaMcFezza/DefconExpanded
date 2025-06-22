/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "AutoTagWriter.h"

#include <stdlib.h>
#include <string.h>

#include "Lib/Codes.h"

#include "CompressedTag.h"
// #include "Settings.h"
#include "DumpBuff.h"
#include "Main/Log.h"

enum { BUFSIZE = 4096 };

// called when the buffer would overrun and needs expansion
void AutoTagWriter::OnBufferOverrun()
{
    GrowBuffer();
}

void AutoTagWriterBase::InitBuffer( size_t size_ )
{
    buffer = new unsigned char[ size_ ];
    size = size_;
}

AutoTagWriterBase::~AutoTagWriterBase()
{
    delete[] buffer;
    buffer = 0;
}

AutoTagWriterBase::AutoTagWriterBase( size_t size_ )
{
    InitBuffer( size_ );
    size = size_;
}


void AutoTagWriter::GrowBuffer()
{
    unsigned char * oldBuffer = buffer;
    int oldSize = size;

    // increase left counter
    left += oldSize;

    // create new buffer, copy old content
    InitBuffer( oldSize * 2 );

    memcpy( buffer, oldBuffer, oldSize );

    // offset pointers
    int offset   = buffer - oldBuffer;
    target      += offset;
    atomCount   += offset;
    nestedCount += offset;

    delete[] oldBuffer;
}

//! @param type the type of the tag to write
AutoTagWriter::AutoTagWriter( char const * type )
: AutoTagWriterBase( BUFSIZE ), TagWriter( buffer, BUFSIZE, type )
{
}

//! @param type the type of the tag to write
//! @param clas the class of the tag to write (that's the first atom of the form C_TAG_TYPE=class)
AutoTagWriter::AutoTagWriter( char const * type, char const * clas, char const * typetag )
: AutoTagWriterBase( BUFSIZE ), TagWriter( buffer, BUFSIZE, type )
{
    buffer = target - BUFSIZE + BytesLeft();

    if ( !typetag )
    {
        typetag = C_TAG_TYPE;
    }
    WriteString( typetag, clas );
}

int AutoTagWriter::Size() const
{
    return target - buffer;
}

// stores the buffer for later use
void AutoTagWriter::Compress( CompressedTag & compressed ) const
{
    // make sure everything is wrapped up
    Close();

    // and copy the data over.
    compressed.content.clear();
    compressed.content.reserve( Size() );

    if ( Size() > 0 )
    {
// #ifdef COMPILE_DEDCON
        assert( buffer[Size()-1] == 0 );
// #endif
        assert( buffer[Size()-2] == '>' );
    }

    for( int i = 0; i < (int)Size()-1; ++i )
        compressed.content.push_back( buffer[i] );
}

//! writes contents to file
void AutoTagWriter::Save( std::ostream & s ) const
{
    CompressedTag compressed;
    Compress( compressed );
    compressed.Save( s );
}

void AutoTagWriter::Dump() const
{
    if ( Log::GetLevel() < 3 )
        return;

    Close();

    dumpbuf( buffer, Size() );
}
