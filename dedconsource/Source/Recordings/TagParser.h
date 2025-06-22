/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_TagParser_INCLUDED
#define DEDCON_TagParser_INCLUDED

#include "Lib/Forward.h"

#include <string>
#include <vector>

class TagReader;

//! abstract class that parses whole tags, calling virtual functions to do the real work
class TagParser
{
    friend class TagReader;
public:
    TagParser(){}
    
    //! parse a tag
    void Parse( TagReader & reader );

protected:
    virtual ~TagParser(){};

    //! called when the type of the tag is read (happens only if the reader was not started yet)
    virtual void OnType( std::string const & type ) = 0;

    //! called when value-key pairs of the various types are encountered. default action: ignore.
    virtual void OnChar( std::string const & key, int value ) = 0;
    virtual void OnInt( std::string const & key, int value ) = 0;
    virtual void OnLong( std::string const & key, LongLong const & value ) = 0;
    virtual void OnIntArray( std::string const & key, std::vector<int> const & array ) = 0;
    virtual void OnFloat( std::string const & key, float value ) = 0;
    virtual void OnString( std::string const & key, std::string const & value ) = 0;
    virtual void OnUnicodeString( std::string const & key, std::wstring const & value ) = 0;
    virtual void OnBool( std::string const & key, bool value ) = 0;
    virtual void OnDouble( std::string const & key, double const & value ) = 0;
    virtual void OnRaw( std::string const & key, unsigned char const * data, int len ) = 0;

    //! checkpoints
    virtual void OnStart( TagReader & reader ) = 0;
    virtual void AfterStart( TagReader & reader ) = 0;
    virtual void BeforeNest( TagReader & reader ) = 0;
    virtual void OnFinish( TagReader & reader ) = 0;

    //! called for nested tags
    virtual void OnNested( TagReader & reader ) = 0;

private:
    TagParser( TagParser const & );
    TagParser & operator = ( TagParser const & );
};

//! abstract class that parses whole tags, calling virtual functions to do the real work
class TagParserLax: public TagParser
{
    friend class TagReader;
public:
    TagParserLax(){}
    
protected:
    virtual ~TagParserLax(){};

    //! called when the type of the tag is read (happens only if the reader was not started yet)
    virtual void OnType( std::string const & type ) {}

    //! called when value-key pairs of the various types are encountered. default action: ignore.
    virtual void OnChar( std::string const & key, int value ){}
    virtual void OnInt( std::string const & key, int value ){}
    virtual void OnLong( std::string const & key, LongLong const & value ){}
    virtual void OnIntArray( std::string const & key, std::vector<int> const & array ){}
    virtual void OnFloat( std::string const & key, float value ){}
    virtual void OnString( std::string const & key, std::string const & value ){}
    virtual void OnUnicodeString( std::string const & key, std::wstring const & value );
    virtual void OnBool( std::string const & key, bool value ){}
    virtual void OnDouble( std::string const & key, double const & value ){}
    virtual void OnRaw( std::string const & key, unsigned char const * data, int len ){}

    //! checkpoints
    virtual void OnStart( TagReader & reader ){}
    virtual void AfterStart( TagReader & reader ){}
    virtual void BeforeNest( TagReader & reader ){}
    virtual void OnFinish( TagReader & reader ){}
};

//! tag parser that ignores everything
class TagParserIgnore: public TagParserLax
{
public:
    // called for nested tags
    virtual void OnNested( TagReader & reader )
    {
        // ignore nested tags, too
        TagParserIgnore ignorer;
        ignorer.Parse( reader );
    }
};

#endif // DEDCON_TagParser_INCLUDED
