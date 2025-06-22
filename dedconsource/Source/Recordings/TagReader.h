/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_TagReader_INCLUDED
#define DEDCON_TagReader_INCLUDED

#include "Lib/Misc.h"
#include "Lib/Forward.h"
#include <vector>

//! provides functions to read tags
class TagReader
{
    friend class TagParser;
    friend class CompressedTag;
public:
    //! prepares to read from the given raw buffer
    TagReader( const unsigned char * buffer, int size )
    : read( buffer ), left ( size ), atomsLeft(-1), nestedLeft(-1), obsolete( false ), parent(0) {}

    //! prepares to read from the compressed buffer
    TagReader( CompressedTag const & tag );

    virtual ~TagReader()
    {
        Finish();
    }

    //! the number of atoms left to read
    int AtomsLeft() const { assert( atomsLeft >= 0 ); return atomsLeft; }

    //! the number of nested tags left to read
    int NestedLeft() const { assert( nestedLeft >= 0 ); return nestedLeft; }

    //! the number of bytes left to read
    int BytesLeft(){ return left; }

    //! start reading
    std::string Start();

    //! started already?
    bool Started() const;

    //! start to read nested tags
    void Nest();
    
    //! mark this reader as obsolete; no reads can be done from it in the future, and the destructor won't complain about not having read everything.
    void Obsolete();

    //! finish reading
    void Finish();

    //! help the parser parse anything
    void ParseAny( TagParser & parser, std::string const & key );

    //! read any value and ignore the result
    void ReadAny();

    //! read a byte
    unsigned char ReadChar();
    //! read a boolean
    bool ReadBool();
    //! read a word
    unsigned short int ReadWord();

    //! read an integer value
    unsigned int ReadInt();

    //! read scores
    void ReadIntArray( std::vector< int > & );

    //! read a float value
    float ReadFloat();

    //! read a string
    std::string ReadString();

    //! read a string encoded in UTF16
    std::wstring ReadUnicodeString();
    
    //! read coordinates?
    double ReadDouble();

    //! read time?
    LongLong ReadLong();

    //! read the key part of an atom
    std::string ReadKey();

    //! read raw data
    void ReadRaw( int & len, unsigned char const * & data );

    //! dump the contents
    virtual void Dump();
protected:
    //! prepares to take over reading from another reader
    TagReader( TagReader & other, bool nested );

private:
    TagReader();
    TagReader & operator = ( TagReader const & );
    TagReader( TagReader const & other );

    // unsafe private functions
    inline void CheckLeft( int size );
    inline unsigned char ReadRawCharUnsafe();
    inline unsigned short int ReadRawWordUnsafe();

    // unchecked read operations
    unsigned char ReadRawChar();
    unsigned short int ReadRawWord();
    unsigned int ReadRawInt();
    void ReadRawIntArray( std::vector<int> & );
    float ReadRawFloat();
    double ReadRawDouble();
    LongLong ReadRawLong();
    char const * ReadRawString();
    std::wstring ReadRawUnicodeString();
    void ReadRawRaw( int & len, unsigned char const * & data );

    const unsigned char * read; //!< read position
    int left;                   //!< bytes left to read
    int atomsLeft;              //!< number of atoms left to read
    int nestedLeft;             //!< number of nested tags left to read
    bool obsolete;              //!< has another reader taken over?
    TagReader * parent;         //!< parent tag reader
};

//! reader for nested tags
class NestedTagReader: public TagReader
{
public:
    //! reads a nested tag of the parent reader
    NestedTagReader( TagReader & parent )
    : TagReader( parent, true ){}
};

// independent reader copies
class TagReaderCopy: public TagReader
{
public:
    //! takes over where the parent reader stopped; this and the parent reader can operate independently.
    TagReaderCopy( TagReader & parent )
    : TagReader( parent, false ){}
};

#endif // DEDCON_TagReader_INCLUDED
