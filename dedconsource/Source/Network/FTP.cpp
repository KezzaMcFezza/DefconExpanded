/*
 *  * DedCon: Dedicated Server for Defcon (and Multiwinia)
 *  *
 *  * Copyright (C) 2008 Manuel Moos
 *  *
 *  */

#include "FTP.h"
#include "Main/Log.h"
#include "Server/Client.h"
#include "Lib/Codes.h"
#include "Recordings/AutoTagWriter.h"
#include "Network/Socket.h"
#include "Lib/Globals.h"
#include "Recordings/GameHistory.h"

#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <deque>

#ifndef COMPILE_DEDCON

// FTP parser
class FTPParser: public TagParserLax
{
public:
    std::vector< unsigned char > blockData; //!< the data block sent
    std::vector< int > acks;                //!< the acked blocks
    std::string fileName;          //!< the name of the file
    int fileSize;                  //!< the size of the file
    int blockNumber;               //!< the block number

    FTPParser()
    : fileSize( -1 ), blockNumber( -1 ){}

    virtual void OnInt( std::string const & key, int value )
    {
        if ( key == C_FTP_FILESIZE )
        {
            fileSize = value;
        }
        else if ( key == C_FTP_BLOCKNUMBER )
        {
            blockNumber = value;
        }
    }

    virtual void OnIntArray( std::string const & key, std::vector<int> const & array )
    {
        if ( key == C_FTP_BLOCKNUMBER )
        {
            acks = array;
        }
    }

    virtual void OnString( std::string const & key, std::string const & value )
    {
        if ( key == C_FTP_FILENAME )
        {
            fileName = value;
        }
    }

    virtual void OnRaw( std::string const & key, unsigned char const * data, int len )
    {
        if ( key == C_FTP_BLOCKDATA )
        {
            blockData.clear();
            blockData.reserve( len );
            while( len > 0 )
            {
                blockData.push_back( *(data++) );
                --len;
            }
        }
    }
    
    virtual void OnNested( TagReader & reader ){}
};

// data block
class Block
{
public:
    Block()
    : id( -1 ){}

    void Fill( FTPParser const & parser )
    {
        assert( parser.blockNumber >= 0 );

        id = parser.blockNumber;
        data = parser.blockData;
    }

    void Fill( int id, std::istream & stream, size_t max_len );

    int id;
    std::vector< unsigned char > data;
};

// a complete file
class FTPFile
{
public:
    FTPFile()
    : size(-1), broadcast( false ), used( false )
    {}

    // the individual blocks
    std::map< int, Block > blocks;

    // the filename
    std::string name;

    int size;
    
    // whether the file should be broadcasted to all clients
    bool broadcast; 

    // whether the file has already been sent to someone
    bool used;
};

// manages sending file blocks
class BlockSender
{
public:
    Block const & block;
    Client & receiver;
    FTPFile & file;
    Time lastSent;
    bool ack;

    BlockSender( Client & receiver_, Block const & block_, FTPFile & file_ )
    : block( block_ ), receiver( receiver_ ), file( file_ ), ack( false )
    {
        lastSent.GetTime();
        lastSent.AddSeconds(-2);
    }
};

class FTPClient
{
public:
    // blocks that need sending
    std::deque< BlockSender > todo;
};

// all files. We keep them around for the whole runtime,
// so we don't have to worry about memory management
std::map< std::string, FTPFile > files;

// flag indicating whether all transfers are complete
static bool complete = false;

//! sends pending FTP packets
void FTP::Send()
{
    Time now;
    now.GetTime();
    static Time last = now;
    if ( (now - last).Milliseconds() < 100 )
        return;
    last = now;

    complete = true;

    for( Client::const_iterator iter = Client::GetFirstClient(); iter; ++iter )
    {
        if ( !iter->ftpClient )
        {
            continue;
        }

        std::deque< BlockSender > & todo = iter->ftpClient->todo;
        
        bool goon = true;
        while( goon && todo.size() > 0 )
        {
            BlockSender & sender = todo.front();
            FTPFile & file = sender.file;

            complete = false;

            // bandwidth available?
            if ( sender.receiver.bandwidthControl <= 0 )
            {
                // no. reschedule packet for later.
                goon = false;
                break;
            }

            if ( sender.ack || sender.receiver.id.GetState() == SuperPower::NonEx )
            {
                // packet has been acknowledged or receiver quit. Remove it.
                todo.pop_front();
            }
            else
            {
                Time since = now - sender.lastSent;
                if ( since.Milliseconds() > 1000 )
                {
                    // resend the packet and sort it in at the back
                    AutoTagWriter writer( C_TAG, C_FTP_SEND );
                    writer.WriteInt( C_SEQID_CURRENT, static_cast<unsigned int>(-1) );
                    writer.WriteString( C_FTP_FILENAME, file.name );
                    writer.WriteInt( C_FTP_FILESIZE, file.size );
                    writer.WriteInt( C_FTP_BLOCKNUMBER, sender.block.id );
                    writer.WriteRaw( C_FTP_BLOCKDATA, &*sender.block.data.begin(), sender.block.data.size() );
                    writer.WriteInt( C_SEQID_LAST, gameHistory.GetTailSeqID() );

                    Log::Out() << "Sending chunk " << sender.block.id << " of file " << file.name << ".\n";

                    // wrap up and send
                    writer.Close();
                    sender.receiver.bandwidthControl -= writer.Size() + packetOverhead.Get();
                    writer.SendTo( iter->serverSocket, sender.receiver.saddr, PLEBIAN_ENCRYPTION );

                    sender.lastSent = now;
                    todo.push_back( sender );
                    todo.pop_front();
                }
                else
                {
                    todo.push_back( sender );
                    todo.pop_front();
                    goon = false;
                }
            }
        }
    }
}

StringSetting ftpPrefix( SettingBase::Log, "FTPPrefix" );

//! processes an FTP packet (if broadcast is true, the file is
//! mirrored to all connected clients)
void FTP::Process( Client & sender, 
                   TagReader & reader,
                   bool broadcast )
{
    // parse it
    FTPParser parser;
    parser.Parse( reader );

    // it is a data block
    FTPFile & file = files[ parser.fileName ];

    file.used = true;

    // check type
    if ( parser.blockNumber >= 0 )
    {
        Block & block = file.blocks[ parser.blockNumber ];
        if ( file.size < 0 )
        {
            file.size = parser.fileSize;
            file.name =parser.fileName;
        }

        if( Log::GetLevel() >= 3 )
        {
            Log::Out() << "Received block " << parser.blockNumber
                       << " of file \"" << parser.fileName
                       << "\".\n";
        }

        // send acknowledgement
        AutoTagWriter writer( C_TAG, C_FTP_SEND );
        writer.WriteInt( C_SEQID_CURRENT, static_cast<unsigned int>(-1) );
        writer.WriteString( C_FTP_FILENAME, parser.fileName );
        std::vector<int> ack;
        ack.push_back( parser.blockNumber );
        writer.WriteIntArray( C_FTP_BLOCKNUMBER, ack );

        writer.SendTo( serverSocket, sender.saddr, PLEBIAN_ENCRYPTION );

        // already received?
        if ( block.data.size() > 0 )
        {
            return;
        }

        // no. Store it.
        block.Fill( parser );

        if ( file.size != parser.fileSize )
        {
            Log::Err() << "Ignoring FTP packet, file size changed.\n";
        }

        if ( broadcast )
        {
            // prepare sending to other clients
            for( Client::iterator i = Client::GetFirstClient(); i; ++i )
            {
                if( &*i != &sender )
                {
                    if ( !i->ftpClient )
                    {
                        i->ftpClient = new FTPClient;
                    }
                    i->ftpClient->todo.push_back( BlockSender( *i, block, file ) );
                }
            }
        }

        // check if the file is complete
        int totalSize = 0;
        for( std::map< int, Block >::const_iterator i = file.blocks.begin();
             i != file.blocks.end(); ++i )
        {
            totalSize += (*i).second.data.size();
        }
        if( totalSize >= parser.fileSize )
        {
            if( Log::GetLevel() >= 2 )
            {
                Log::Out() << "Received file \"" << parser.fileName << "\".\n";
            }
            Log::Event() << "FTP_RECEIVE " << parser.fileName << "\n";

            if ( ftpPrefix.IsDefault() )
            {
                return;
            }

            std::ostringstream f;
            f << ftpPrefix.Get() << parser.fileName;
            std::ofstream s( f.str().c_str() );
            for( std::map< int, Block >::const_iterator i = file.blocks.begin();
                 i != file.blocks.end(); ++i )
            {
                std::vector< unsigned char > const & data = (*i).second.data;
                s.write( reinterpret_cast< char const * >( &*data.begin() ), data.size() );
            }
        }
    }
    else
    {
        if ( !sender.ftpClient )
        {
            return;
        }

        // it is an ack packet
        for( std::deque< BlockSender >::iterator i = sender.ftpClient->todo.begin();
             i != sender.ftpClient->todo.end(); ++i )
        {
            // mark packets as acked
            if ( &(*i).receiver == &sender && 
                 std::find( parser.acks.begin(), parser.acks.end(),
                            (*i).block.id )
                 != parser.acks.end() )
            {
                (*i).ack = true;
            }
        }
    }
}

bool FTP::Complete( Client * client )
{
    if ( ! client )
    {
        return complete;
    }
    else
    {
        return !client->ftpClient || client->ftpClient->todo.size() == 0;
    }
}

//! loads a file and preparses it for sending
FTPFile * FTP::LoadFile( char const * filename, char const * officialName, bool broadcast )
{
    // open file
    std::ifstream s( filename );

    if ( !s.good() )
    {
        // fail
        return 0;
    }
    
    // read file in small, handy chunks (we don't want to strain the
    // crypto system with large packets)
    // this is a MAGIC number. MW doesn't accept any other block size.
    unsigned int chunkSize = 1024;

    // create file
    FTPFile & file = files[ officialName ];

    if ( file.size > 0 )
    {
        // filename already taken
        if ( file.used )
        {
            // and already used. Can't cancel that.
            return 0;
        }
        else
        {
            // clear it
            file = FTPFile();
        }
    }

    file.broadcast = broadcast;
    file.name = officialName;
    
    int currentID = 0;
    Block * block = 0;

    int c = s.get();
    int size = 0;

    while( c >= 0 )
    {
        // open new block
        if ( !block || block->data.size() >= chunkSize )
        {
            block = &file.blocks[currentID];
            block->data.reserve( chunkSize );
            block->id = currentID;
            ++currentID;
        }
        
        // append byte
        block->data.push_back( c );
        ++size;

        // and fetch new one
        c = s.get();
    }

    file.size = size;
    return & file;
}

//! sends an existing file to a client
void FTP::SendFile( FTPFile * file, Client & client )
{
    if ( !file )
    {
        return;
    }

    file->used = true;

    if ( !client.ftpClient )
    {
        client.ftpClient = new FTPClient;
    }

    // just add new block senders
    for( std::map< int, Block >::const_iterator i = file->blocks.begin();
         i != file->blocks.end(); ++i )
    {
        client.ftpClient->todo.push_back( BlockSender( client, (*i).second, *file ) );
    }
}

void FTP::DeleteFTPClient( FTPClient * client )
{
    delete client;
}
#else
void FTP::Send()
{
}

//! loads a file and preparses it for sending
FTPFile * FTP::LoadFile( char const * filename, char const * officialName, bool broadcast )
{
    return 0;
}

//! sends an existing file to a client
void FTP::SendFile( FTPFile * file, Client & client )
{
}

bool FTP::Complete( Client * client )
{
    return true;
}

void FTP::DeleteFTPClient( FTPClient * client )
{
}
#endif
