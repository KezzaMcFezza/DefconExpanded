/*
 *  * DedCon: Dedicated Server for Defcon (and Multiwinia)
 *  *
 *  * Copyright (C) 2008 Manuel Moos
 *  *
 *  */

#include "config.h"

#ifdef HAVE_ZLIB_H
#include <zlib.h>

#include "Log.h"
#include "TagReader.h"
#include "TagParser.h"
#include "TagWriter.h"
#include "AutoTagWriter.h"
#include "DumpBuff.h"

#include <iostream>
#include <fstream>
#include <sstream>

#include <stdlib.h>
#include <cstring>

static bool processBmps = false;

class TagParserMap: public TagParserLax
{
public:
    explicit TagParserMap( std::string const & prefix_, TagWriter * repack_ ): prefix( prefix_ ), repack( repack_ ){}
    
    //! called when the type of the tag is read (happens only if the reader was not started yet)
    virtual void OnType( std::string const & type )
    {
        if ( repack )
        {
            repack->Open( type.c_str() );
        }
    }

    virtual void OnRaw( std::string const & key, unsigned char const * data, int len )
    {
        Log::Err() << key << " = (" << len << ")\n";

        std::string filename = key;
        bool process = processBmps;

        if ( key == "MapData" )
        {
            filename = prefix + ".txt";
            process = true;
        }
        else if ( key == "MapThumbnail" )
        {
            filename = prefix + ".bmp";
        }
        else
        {
            filename = prefix + "_" + key + ".bmp";
        }

        if ( repack )
        {
            bool repacked = false;
            if ( process )
            {
                std::ostringstream o;
                std::ifstream f( filename.c_str() );
                if ( f.good() )
                {
                    int c = f.get();
                    do
                    {
                        o << (char)( c );
                        c = f.get();
                    } while( c >= 0 );
                }

                repack->WriteRaw( key, (unsigned char const *)o.str().c_str(), o.str().size() );
                
                repacked = true;
            }

            if ( !repacked )
            {
                repack->WriteRaw( key, data, len );
            }
        }
        else if ( process )
        {
            std::ofstream s( filename.c_str() );
            s.write( (char const *)data, len );
        }
    }

    virtual void OnInt( std::string const & key, int data )
    {
        Log::Err() << key << " = " << data << "\n";

        if ( repack )
        {
            repack->WriteInt( key, data );
        }
    }

    virtual void OnString( std::string const & key, std::string const & data )
    {
        Log::Err() << key << " = " << data << "\n";

        if ( repack )
        {
            repack->WriteString( key, data );
        }
    }

    virtual void OnNested( TagReader & reader )
    {
    }

    // filename prefix
    std::string prefix;

    // writer to repack the map to
    TagWriter * repack;

};

void ProcessMap( std::string const & compressed, std::string const & prefix, bool extract )
{
    unsigned char const * data = (unsigned char const *)compressed.c_str();
    size_t len = compressed.size();

    if ( len <= 4 )
    {
        Log::Err() << "Error during uncompression: .mwm file too short.\n";
    }

    // read uncompressed data size
    size_t uncompressedSize = *(data++);
    uncompressedSize += *(data++) << 8;
    uncompressedSize += *(data++) << 16;
    uncompressedSize += *(data++) << 24;
    len -= 4;

    // allocate buffer
    unsigned char * buffer = new unsigned char[ uncompressedSize ];
    
    // uncompress
    uLongf realsize = uncompressedSize;
    int err = uncompress( (Bytef*)buffer, &realsize, (Bytef*)data, len );
    if ( err != Z_OK || uncompressedSize != realsize )
    {
        Log::Err() << "Error during uncompression: " << err << ", " << realsize << ", " << uncompressedSize << ".\n";
        delete[] buffer;
        return;
    }
    Log::Err() << "Uncompression: " << err << ", " << realsize << ", " << uncompressedSize << ".\n";

    // dumpbuf( buffer, realsize );
    TagReader reader( buffer, realsize );
    if ( extract )
    {
        TagParserMap parser( prefix, 0 );
        parser.Parse( reader );
    }
    else
    {
        AutoTagWriter rewrite;
        TagParserMap parser( prefix, &rewrite );
        parser.Parse( reader );
        rewrite.Close();

        // compress data
        size_t bufflen = 2 * rewrite.Size() + 100;
        uLongf realbufflen = bufflen;
        char * buffer = new char[ bufflen + 4 ];
        int size = rewrite.Size() - 1;
        buffer[0] = size & 0xff;
        buffer[1] = ( size >> 8 ) & 0xff;
        buffer[2] = ( size >> 16 ) & 0xff;
        buffer[3] = ( size >> 24 ) & 0xff;
        
        int result = compress2( (Bytef *)(buffer + 4), &realbufflen,
                                (Bytef *)rewrite.GetBuffer(), size,
                                9 );
        
        if ( result != Z_OK )
        {
            Log::Err() << "Unable to compress data.\n";
            delete[] buffer;
            exit(1);
        }

        std::string ofilename = prefix + "_repack.mwm";
        std::ofstream o( ofilename.c_str() );
        if ( !o.good() )
        {
            Log::Err() << "Unable to open output file " << ofilename << ".\n";
            delete[] buffer;
            exit(1);
        }

        o.write( buffer, realbufflen + 4 );

        delete[] buffer;
    }
    reader.Obsolete();

    delete[] buffer;
}


void ProcessMap( std::string const & filename, bool extract )
{
    std::string prefix( filename );
    if( prefix.size() > 4 && filename[ prefix.size() - 4 ] == '.' )
    {
        prefix = std::string( prefix, 0, prefix.size() - 4 );
    }

    std::ostringstream o;
    
    {
        std::ifstream f( filename.c_str() );
        if ( !f.good() )
        {
            Log::Err() << "Errro opening " << filename << ".\n";
            exit(1);
        }

        int c = f.get();
        do
        {
            o << (char)( c );
            c = f.get();
        } while( c >= 0 );
    }

    ProcessMap( o.str(), prefix, extract );
}


int main( int argc, char *argv[] )
{
    if ( argc <= 1 )
    {
        Log::Err() << "Usage: " << argv[0] << " <mapname>\n";
        return 1;
    }

    std::string filename;

    bool extract = true;
    bool noOption = false;

    for( int i = 1; i < argc; ++i )
    {
        if ( strcmp( argv[i], "-a" ) == 0 )
        {
            processBmps = true;
        }
        else if ( strcmp( argv[i], "-r" ) == 0 )
        {
            extract = false;
        }
        else if ( strcmp( argv[i], "--" ) == 0 )
        {
            noOption = true;
        }
        else if ( !noOption && argv[i][0] == '-' )
        {
            Log::Err() << "Unknown option " << argv[i] << ".\n";
            return 1;
        }
        else
        {
            filename = argv[i];
        }
    }

    if ( filename.size() > 0 )
    {
        ProcessMap( filename, extract );
    }

    return 0;
}

#else // HAVE_ZLIB_H

int main( int argc, char *argv[] )
{
    return 0;
}

#endif
