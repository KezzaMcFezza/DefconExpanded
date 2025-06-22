/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "TagParser.h"

#include "TagReader.h"
#include "Network/Exception.h"
#include "Lib/Unicode.h"

// parse a tag
void TagParser::Parse( TagReader & reader )
{
    try
    {
        // start reading
        if ( !reader.Started() )
        {
            OnStart( reader );
            OnType( reader.Start() );
            AfterStart( reader );
        }

        // read atoms
        while( reader.AtomsLeft() > 0 )
        {
            std::string key = reader.ReadKey();
            reader.ParseAny( *this, key );
        }

        BeforeNest( reader );

        // read nested tags
        reader.Nest();
        while( reader.NestedLeft() > 0 )
        {
            TagReader nested( reader, true );
            try
            {
                OnNested( nested );
            }
            catch ( ReadException const & e )
            {
                nested.Obsolete();

                throw;
            }
        }

        OnFinish( reader );
    }
    catch(...)
    {
        reader.Obsolete();
        throw;
    }
}

void TagParserLax::OnUnicodeString( std::string const & key, std::wstring const & value )
{ 
    OnString( key, Unicode::Convert( value ) );
}

