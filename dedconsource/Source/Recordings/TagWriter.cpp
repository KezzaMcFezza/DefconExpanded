/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "TagWriter.h"

#include <stdlib.h>
#include <string.h>

#include "Lib/Codes.h"

#include "Lib/Misc.h"
#include "CompressedTag.h"
#include "Network/Exception.h"
#include "Lib/Unicode.h"
#include "Main/Log.h"

//! prepares to write into the specified raw buffer
TagWriter::TagWriter( unsigned char * buf, int len, char const * type )
        : target( buf ), left( len ), nestedCount( 0 ), parent( 0 ), state( Raw )
{
    if ( type )
        Open( type );
}

TagWriter::~TagWriter()
{
    Close();
}

// called when the buffer would overrun and needs expansion
void TagWriter::OnBufferOverrun()
{
    Log::Err() << "TagWriter buffer overrun.\n";
    throw QuitException( 1 );
}

//! create a tag writer that writes nested tags
TagWriter::TagWriter( TagWriter & parent_, char const * type )
        : target( 0 ), left( 0 ), nestedCount( 0 ), parent( 0 ), state( Raw )
{
    parent = &parent_;

    // make preparations for nesting
    PrepareParent( parent );

    target = parent->target;
    left = parent->left;

    // and open this tag.
    if ( type )
        Open( type );
}

//! opens the tag
void TagWriter::Open( char const * type )
{
    assert( type );
    assert( state == Raw );

    // write the start of the tag
    WriteRawChar( '<' );
    WriteRawString( type );
    atomCount = target;
    WriteRawChar( 0 );

    state = Atoms;
}

//! closes the tag
void TagWriter::Close() const
{
    const_cast< TagWriter * >( this )->CloseNonConst();
}

//! forgets about this writer, pretend it never existed
void TagWriter::Obsolete()
{
    assert( state == Raw );
    state = Complete;

    // undo nested count increase
    if ( parent )
        (*parent->nestedCount)--;
}

//! writes a compressed tag as a nested tag
void TagWriter::WriteCompressed( CompressedTag const & compressed )
{
    // make preparations for nesting
    PrepareParent( this );

    // temporarily mess with the state
    state = Atoms;

    // and simply write the buffer.
    for( std::vector< unsigned char >::const_iterator
            iter = compressed.content.begin();
            iter != compressed.content.end();
            ++iter )
        WriteRawChar( (*iter) );

    state = Nested;
}

//! write a char valued key-value pair
void TagWriter::WriteChar( char const * key, unsigned char c )
{
    WriteKey(key);
    WriteRawChar(3);
    WriteRawChar(c);
}

//! write a boolean valued key-value pair
void TagWriter::WriteBool( char const * key, bool b )
{
    WriteKey(key);
    WriteRawChar(5);
    WriteRawChar(b);
}

//! write a word valued key-value pair
void TagWriter::WriteWord( char const * key, unsigned short int w )
{
    WriteKey(key);
    // guessing again
    WriteRawChar(3);
    WriteRawWord(w);
}

//! write an integer valued key-value pair
void TagWriter::WriteInt( char const * key, unsigned int i )
{
    WriteKey(key);
    WriteRawChar(1);
    WriteRawInt(i);
}

//! write a coordinate valued key-value pair
void TagWriter::WriteLong( char const * key, LongLong const & v )
{
    WriteKey(key);
    WriteRawChar(9);
    WriteRawLong(v);
}

//! write an integer array valued key-value pair
void TagWriter::WriteIntArray( char const * key, std::vector<int> const & a )
{
    WriteKey(key);
    WriteRawChar(8);
    WriteRawIntArray(a);
}

//! write a float valued key-value pair
void TagWriter::WriteFloat( char const * key, float f )
{
    WriteKey(key);
    WriteRawChar(2);
    WriteRawFloat(f);
}

//! write a string valued key-value pair
void TagWriter::WriteString( char const * key, char const * c )
{
    WriteKey(key);
    WriteRawChar(4);
    WriteRawString(c);
}

//! write a string valued key-value pair
void TagWriter::WriteUnicodeString( char const * key, wchar_t const * c )
{
    WriteKey(key);
    WriteRawChar(10);
    WriteRawUnicodeString(c);
}

//! writes a chunk of raw data
void TagWriter::WriteRaw( char const * key, unsigned char const * data, int len )
{
    WriteKey(key);
    WriteRawChar(6);
    WriteRawRaw( data, len );
}

//! write the type of a tag
void TagWriter::WriteClass( char const * type )
{
    WriteString( C_TAG_TYPE, type );
}

//! write a coordinate valued key-value pair
void TagWriter::WriteDouble( char const * key, double const & v )
{
    WriteKey(key);
    WriteRawChar(7);
    WriteRawDouble(v);
}

//! write a string valued key-value pair
void TagWriter::WriteString( char const * key, std::string const & s )
{
    WriteString( key, s.c_str() );
}

//! write a string valued key-value pair, as wstring in dedwinia and regular string in dedcon
void TagWriter::WriteFlexString( char const * key, std::string const & s )
{
#ifdef COMPILE_DEDCON
    WriteString( key, s );
#else
    WriteUnicodeString( key, Unicode::Convert( s ) );
#endif
}

//! write a string valued key-value pair
void TagWriter::WriteUnicodeString( char const * key, std::wstring const & s )
{
    WriteUnicodeString( key, s.c_str() );
}

//! write a char valued key-value pair
void TagWriter::WriteChar( std::string const & key, unsigned char c )
{
    WriteChar( key.c_str(), c );
}

//! write a boolean valued key-value pair
void TagWriter::WriteBool( std::string const & key, bool b )
{
    WriteBool( key.c_str(), b );
}

//! write a word valued key-value pair
void TagWriter::WriteWord( std::string const & key, unsigned short int w )
{
    WriteWord( key.c_str(), w );
}

//! write an integer valued key-value pair
void TagWriter::WriteInt( std::string const & key, unsigned int i )
{
    WriteInt( key.c_str(), i );
}

//! write a coordinate valued key-value pair
void TagWriter::WriteLong( std::string const & key, LongLong const & v )
{
    WriteLong( key.c_str(), v );
}

//! write an integer array valued key-value pair
void TagWriter::WriteIntArray( std::string const & key, std::vector<int> const & a )
{
    WriteIntArray( key.c_str(), a );
}

//! write a float valued key-value pair
void TagWriter::WriteFloat( std::string const & key, float f )
{
    WriteFloat( key.c_str(), f );
}

//! write a string valued key-value pair
void TagWriter::WriteString( std::string const & key, char const * c )
{
    WriteString( key.c_str(), c );
}

//! write a string valued key-value pair
void TagWriter::WriteUnicodeString( std::string const & key, wchar_t const * c )
{
    WriteUnicodeString( key.c_str(), c );
}

//! write a coordinate valued key-value pair
void TagWriter::WriteDouble( std::string const & key, double const & v )
{
    WriteDouble( key.c_str(), v );
}

//! write a string valued key-value pair
void TagWriter::WriteString( std::string const & key, std::string const & s )
{
    WriteString( key.c_str(), s.c_str() );
}

//! write a string valued key-value pair
void TagWriter::WriteUnicodeString( std::string const & key, std::wstring const & s )
{
    WriteUnicodeString( key.c_str(), s.c_str() );
}

//! writes a chunk of raw data
void TagWriter::WriteRaw( std::string const & key, unsigned char const * data, int len )
{
    WriteRaw( key.c_str(), data, len );
}

//! close up
void TagWriter::CloseNonConst()
{
    switch( state )
    {
    case Raw:
        Open("");
    case Atoms:
        // no nested tags
        WriteRawChar( 0 );
        state = Nested;
    case Nested:
        state = Complete;
        // finish up
        WriteRawChar( '>' );

        // update parent
        if ( parent )
        {
            parent->target = target;
            parent->left   = left;
        }
        else
        {
// #ifdef COMPILE_DEDCON
            // write "end of message" marker
            WriteRawChar( 0 );
// #endif
        }
    case Complete:
        // nothing to do
        break;
    }
}

//! prepare the parent tag for nested writes
void TagWriter::PrepareParent( TagWriter * parent )
{
    assert( parent );

    if ( parent->state == Complete )
        throw WriteException( "can't write nested tags, the parent is closed." );


    if ( !parent->nestedCount )
    {
        // the parent does not have nested tags yet. Start them.
        assert( parent->state == Atoms );
        parent->nestedCount = parent->target;
        parent->WriteRawChar(0);

        parent->state = Nested;
    }

    assert( parent->state == Nested );

    if ( (*parent->nestedCount) >= 255 )
        throw WriteException("too many nested tags");

    // increase nesting count
    (*parent->nestedCount)++;
}

//! writes a key
void TagWriter::WriteKey( char const * key )
{
    if ( (*atomCount) >= 255 )
        throw WriteException("too many atoms");

    if ( state != Atoms )
        throw WriteException("can't write key-value pairs in this state");

    WriteRawString(key);
    (*atomCount)++;
}

// raw write functions
void TagWriter::WriteRawChar( unsigned char c )
{
    if ( state == Nested )
        throw WriteException("can't write while nested writes are active");

    if( left == 0 )
    {
        OnBufferOverrun();
    }
    assert( left > 0 );

    --left;
    *(target++) = c;
}

void TagWriter::WriteRawWord( unsigned short int w )
{
    WriteRawChar( w & 0xff );
    WriteRawChar( ( w & 0xff00 ) >> 8 );
}

void TagWriter::WriteRawInt( unsigned int i )
{
    WriteRawWord( i & 0xffff );
    WriteRawWord( ( i & 0xffff0000 ) >> 16 );
}

void TagWriter::WriteRawIntArray( std::vector<int> const & a )
{
    assert( a.size() < 256 );
    WriteRawChar( a.size() );
    for( std::vector<int>::const_iterator i = a.begin(); i != a.end(); ++i )
    {
        WriteRawInt( *i );
    }
}
void TagWriter::WriteRawFloat( float f )
{
    assert( sizeof( float ) == sizeof( int ) );
    union { float f; int i; } u;

    u.f = f;

    WriteRawInt( u.i );
}

void TagWriter::WriteRawString( char const * c )
{
    int len = strlen(c);

    if ( len < 255 )
        WriteRawChar( len );
    else
    {
        WriteRawChar( 255 );
        WriteRawInt( len );
    }
    while ( len-- > 0 )
    {
        // filter out %, that seems to make trouble
        char w = *(c++);
        WriteRawChar( w == '%' ? ' ' : w  );
    }
}

void TagWriter::WriteRawUnicodeString( wchar_t const * c )
{
    int len = wcslen(c);

    if ( len < 255 )
        WriteRawChar( len );
    else
    {
        WriteRawChar( 255 );
        WriteRawInt( len );
    }
    while ( len-- > 0 )
    {
        // filter out %, that seems to make trouble
        wchar_t w = *(c++);
        WriteRawWord( w == '%' ? ' ' : w  );
    }
}

void TagWriter::WriteRawLong( LongLong const & l )
{
    WriteRawInt( l.lower );
    WriteRawInt( l.higher );
}

void TagWriter::WriteRawDouble( double const & d )
{
    DoubleUnion u;

    assert( sizeof(u.l) == sizeof(double) );

    u.d = d;

    WriteRawInt( u.l.lower );
    WriteRawInt( u.l.higher );
}

void TagWriter::WriteRawString( std::string const & s )
{
    WriteRawString( s.c_str() );
}

void TagWriter::WriteRawRaw( unsigned char const * data, int len )
{
    WriteRawInt( len );
    while( --len >= 0 )
    {
        WriteRawChar( *data++ );
    }
}
