/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_TagWriter_INCLUDED
#define DEDCON_TagWriter_INCLUDED

#include "Lib/Forward.h"

#include <string>
#include <vector>

//! class that writes tags.
class TagWriter
{
    friend class CompressedTag;
public:
    //! prepares to write into the specified raw buffer
    TagWriter( unsigned char * buf, int len, char const * type = NULL );

    virtual ~TagWriter();

    // called when the buffer would overrun and needs expansion
    virtual void OnBufferOverrun();

    //! create a tag writer that writes nested tags
    TagWriter( TagWriter & parent_, char const * type = NULL );

    //! returns whether the writer is nested
    bool IsNested()
    {
        return parent;
    }

    //! opens the tag
    void Open( char const * type );

    //! closes the tag
    void Close() const;

    //! forgets about this writer, pretend it never existed
    void Obsolete();
    
    //! writes a compressed tag as a nested tag
    void WriteCompressed( CompressedTag const & compressed );

    //! write a char valued key-value pair
    void WriteChar( char const * key, unsigned char c );

    //! write a boolean valued key-value pair
    void WriteBool( char const * key, bool b );

    //! write a word valued key-value pair
    void WriteWord( char const * key, unsigned short int w );

    //! write an integer valued key-value pair
    void WriteInt( char const * key, unsigned int i );

    //! write a long integer
    void WriteLong( char const * key, LongLong const & i );

    //! write an integer array valued key-value pair
    void WriteIntArray( char const * key, std::vector<int> const & a );

    //! write a float valued key-value pair
    void WriteFloat( char const * key, float f );

    //! write a string valued key-value pair
    void WriteString( char const * key, char const * c );

    //! write a string valued key-value pair
    void WriteUnicodeString( char const * key, wchar_t const * c );

    //! writes a chunk of raw data
    void WriteRaw( char const * key, unsigned char const * data, int len );

    //! write the type of a tag
    void WriteClass( char const * type );

    //! write a coordinate valued key-value pair
    void WriteDouble( char const * key, double const & v );

    //! write a string valued key-value pair
    void WriteString( char const * key, std::string const & s );

    //! write a string valued key-value pair, as wstring in dedwinia and regular string in dedcon
    void WriteFlexString( char const * key, std::string const & s );

    //! write a string valued key-value pair
    void WriteUnicodeString( char const * key, std::wstring const & s );

    //! write a char valued key-value pair
    void WriteChar( std::string const & key, unsigned char c );

    //! write a boolean valued key-value pair
    void WriteBool( std::string const & key, bool b );

    //! write a word valued key-value pair
    void WriteWord( std::string const & key, unsigned short int w );

    //! write an integer valued key-value pair
    void WriteInt( std::string const & key, unsigned int i );

    //! write a long integer
    void WriteLong( std::string const & key, LongLong const & i );

    //! write an integer array valued key-value pair
    void WriteIntArray( std::string const & key, std::vector<int> const & a );

    //! write an integer valued key-value pair
    void WriteFloat( std::string const & key, float f );

    //! write a string valued key-value pair
    void WriteString( std::string const & key, char const * c );

    //! write a string valued key-value pair
    void WriteUnicodeString( std::string const & key, wchar_t const * c );

    //! write a coordinate valued key-value pair
    void WriteDouble( std::string const & key, double const & v );

    //! write a string valued key-value pair
    void WriteString( std::string const & key, std::string const & s );

    //! write a string valued key-value pair
    void WriteUnicodeString( std::string const & key, std::wstring const & s );

    //! writes a chunk of raw data
    void WriteRaw( std::string const & key, unsigned char const * data, int len );

    typedef enum { Raw, Atoms, Nested, Complete } State; //!< the current write state

    //!< returns the current write state
    State GetState() const { return state; }

    int BytesLeft() const { return left; }
protected:
    mutable unsigned char * target;  //!< the write target
    mutable unsigned int left;       //!< number of bytes to write left
    unsigned char * atomCount;       //!< the location of the atom count
    unsigned char * nestedCount;     //!< the location of the count of nested tags
    TagWriter * parent;              //!< the parent writer

    State state; //!< the current write state
private:
    TagWriter();
    TagWriter( TagWriter const & );
    TagWriter & operator =( TagWriter const & );

    //! close up
    void CloseNonConst();

    //! prepare the parent tag for nested writes
    static void PrepareParent( TagWriter * parent );

    //! writes a key
    void WriteKey( char const * key );

    // raw write functions
    void WriteRawChar( unsigned char c );
    void WriteRawWord( unsigned short int w );
    void WriteRawInt( unsigned int i );
    void WriteRawLong( LongLong const & i );
    void WriteRawIntArray( std::vector<int> const & a );
    void WriteRawFloat( float f );
    void WriteRawString( char const * c );
    void WriteRawUnicodeString( wchar_t const * c );
    void WriteRawDouble( double const & v );
    void WriteRawString( std::string const & s );
    void WriteRawRaw( unsigned char const * data, int len );
};

#endif // DEDCON_TagWriter_INCLUDED
