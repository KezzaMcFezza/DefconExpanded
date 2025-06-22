/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "Sequence.h"

#include "Lib/Codes.h"

#include "TagParser.h"
#include "GameHistory.h"
#include "CompressedTag.h"
#include "AutoTagWriter.h"
#include "FileTagReader.h"
#include "TagReader.h"
#include "Network/SyncValue.h"
#include "Server/Client.h"
#include "Main/Log.h"
#include "Network/Exception.h"

// parser that rewrites sequences for sending
class TagParserRewriteSequence: public TagParser
{
public:
    TagParserRewriteSequence( TagWriter & writer_, int seqID_, int lastSeqID_ )
    : writer( writer_ ), seqID( seqID_ ), lastSeqID( lastSeqID_ )
    { l = i = false; }

protected:
    //! called when the type of the tag is read (happens only if the reader was not started yet)
    virtual void OnType( std::string const & type ) {}

    // called when value-key pairs of the various types are encountered.
    virtual void OnChar( std::string const & key, int value )
    {
        // tansfer it to the writer
        writer.WriteChar( key, value );
    }

    virtual void OnInt( std::string const & key, int value )
    {
        // tansfer it to the writer
        writer.WriteInt( key, value );

        // set flags
        if ( key == C_SEQID_LAST )
            l = true;
        else if ( key == C_SEQID_CURRENT )
            i = true;
    }

    virtual void OnLong( std::string const & key, LongLong const & value )
    {
        writer.WriteLong( key, value );
    }

    virtual void OnIntArray( std::string const & key, std::vector<int> const & array )
    {
        writer.WriteIntArray( key, array );
    }

    virtual void OnString( std::string const & key, std::string const & value )
    {
        // tansfer it to the writer
        writer.WriteString( key, value );
    }

    virtual void OnUnicodeString( std::string const & key, std::wstring const & value )
    {
        // tansfer it to the writer
        writer.WriteUnicodeString( key, value );
    }

    virtual void OnBool( std::string const & key, bool value )
    {
        // tansfer it to the writer
        writer.WriteBool( key, value );
    }

    virtual void OnFloat( std::string const & key, float value )
    {
        // tansfer it to the writer
        writer.WriteFloat( key, value );
    }

    virtual void OnDouble( std::string const & key, double const & value )
    {
        // tansfer it to the writer
        writer.WriteDouble( key, value );
    }

    virtual void OnRaw( std::string const & key, unsigned char const * data, int len )
    {
        // tansfer it to the writer
        writer.WriteRaw( key, data, len );
    }

    // checkpoints
    virtual void BeforeNest( TagReader & reader )
    {
        // append potentially missing l and i atoms
        if ( !i )
            writer.WriteInt( C_SEQID_CURRENT, seqID );
        if ( !l && !writer.IsNested() )
	{
            writer.WriteInt( C_SEQID_LAST, lastSeqID );
	}
    }

    //! checkpoints
    virtual void OnStart( TagReader & reader ){}
    virtual void AfterStart( TagReader & reader ){}
    virtual void OnFinish( TagReader & reader ){}

    virtual void OnNested( TagReader & reader )
    {
        // no nesting here
        assert(0);
    }

private:
    TagWriter & writer;    // the writer to fill
    bool l, i;             // flags indicating whether the mentioned tags were present in the processed tag
    int seqID;             // the seqID of the parsed sequence
    int lastSeqID;         // the last seqID
};

Sequence::Sequence( AutoTagWriter const & writer, int seqID_ ): seqID( seqID_ )
{
    InsertServer( writer );
    syncValueVote = new SyncValue();
    size = 0;
}

Sequence::Sequence( int seqID_ ): seqID( seqID_ )
{
    syncValueVote = new SyncValue();
    size = 0;
}

Sequence::~Sequence()
{
    delete syncValueVote;
    syncValueVote = 0;
}

// saves worthy client tags into writer
static void SaveTags( List< CompressedTag > * const tag, std::ostream & s )
{
    if ( !tag )
        return;

    SaveTags( tag->Next(), s );

    if ( tag->clientWhitelist.size() == 0 )
    {
        tag->Save( s );
    }
}

//! security checksum to check integrity of archive.
//! It's not really secure, but it doesn't have to be. Anyone who can read
//! the binary and decode what is done can fake recordings.
class SecChecksum
{
public:
    SecChecksum(): checksum( 4711 ), secretChecksum( 42 ){}

    //! adds the contents of a buffer
    void Add( unsigned char const * buffer, unsigned int len )
    {
        // just do some random nonlinear scrambling
        while( len-- > 0 )
        {
            secretChecksum *= 671;
            secretChecksum += buffer[len];
            secretChecksum ^= 0x4a626f32;

            checksum = checksum + secretChecksum * buffer[len];
        }
    }

    //! adds a string
    void Add( std::string const & s )
    {
        Add( reinterpret_cast< unsigned const char * >( s.c_str() ), s.size() );
    }

    //! adds a compressed tag
    void Add( CompressedTag const & tag )
    {
        Add( tag.Buffer(), tag.Size() );
    }

    //! adds a list of compressed tags
    void Add( List< CompressedTag > * tags )
    {
        if ( !tags )
            return;
        Add( tags->Next() );

        if ( tags->clientWhitelist.size() == 0 )
        {
            Add( *tags );
        }
    }

    unsigned int Get() const
    {
        return checksum;
    }
private:
    unsigned int checksum;
    unsigned int secretChecksum;
};

static void Collapse2( ListBase< CompressedTag > & a, ListBase< CompressedTag > & b )
{
    while( b )
    {
        List< CompressedTag > * bb = b;
        bb->Remove();
        bb->Insert( a );
    }
}

static void Collapse( ListBase< CompressedTag > const & a, ListBase< CompressedTag > const & b )
{
    Collapse2( const_cast< ListBase< CompressedTag > & >(a), const_cast< ListBase< CompressedTag > & >(b) );
}


//! saves sequence to stream
void Sequence::Save( std::ostream & s ) const
{
    Collapse( clientTags, clientTagsBack );

    // check whether it is worth to write anything
    int clientTagCount = 0;
    {
        bool worthWriting = !serverTag.Empty();
        for ( ListBase< CompressedTag >::const_iterator ct = clientTags.begin(); ct; ++ct )
        {
            // only non-private client tags are worth writing
            if ( (*ct).clientWhitelist.size() == 0 )
            {
                worthWriting = true;

                ++clientTagCount;
            }
        }
        if ( !worthWriting )
            return;
    }

    static SecChecksum secChecksum;
    
    // write metainfo
    AutoTagWriter metaWriter("sq");

    // seqID
    metaWriter.WriteInt( C_SEQID_LASTCOMPLETE, seqID );

    // comment
    if ( comment.size() > 0 )
    {
        metaWriter.WriteString( C_TAG_TYPE, comment );
        secChecksum.Add( comment );
    }

    // security checksum
    metaWriter.WriteInt( "vs", secChecksum.Get() );

    // enforce checsum voting, we're not going to get better data.
    if ( syncValueVote && syncValueVote->Majority( syncValue, seqID, true ) )
    {
        delete syncValueVote;
        syncValueVote = 0;
    }

    // syncValue
    if ( !syncValueVote )
    {
        metaWriter.WriteChar( C_SYNC_VALUE, syncValue );
    }

    // number of client tags
    metaWriter.WriteInt( "ct", clientTagCount );

    // save that to the file
    metaWriter.Save( s );

    // save the tags
    serverTag.Save( s );
    SaveTags( clientTags, s );

    secChecksum.Add( serverTag );
    secChecksum.Add( clientTags );
}

//! loads sequence from stream
void Sequence::Load( std::istream & s )
{
    int clientTagCount = -1;
    seqID = -1;

    // read the metainfo
    FileTagReader reader( s );

    if ( s.eof() || !s.good() || s.bad() )
        return;

    static SecChecksum secChecksum;

    if ( reader.Start() != "sq" )
    {
        Log::Err() << "Recording file corrupt.\n";
        throw QuitException( -1 );
    }

    bool secChecksumLoaded = false;
    while ( reader.AtomsLeft() )
    {
        // read key
        std::string key = reader.ReadKey();
        if ( key == C_SEQID_LASTCOMPLETE )
        {
            seqID = reader.ReadInt();
        }
        else if ( key == C_SYNC_VALUE )
        {
            delete syncValueVote;
            syncValueVote = 0;
            syncValue = reader.ReadChar();
        }
        else if ( key == C_TAG_TYPE )
        {
            comment = reader.ReadString();
            secChecksum.Add( comment );
        }
        else if ( key == "vs" )
        {
            secChecksumLoaded = true;
            if ( reader.ReadInt() != secChecksum.Get() )
            {
                static bool warn = true;
                if ( warn )
                {
                    Log::Err() << "Warning: Invalid security checksum, the recording may have been modified.\n";
                    warn = false;
                }
            }
        }
        else if ( key == "ct" )
        {
            clientTagCount = reader.ReadInt();
        }
        else
        {
            reader.ReadAny();
        }
    }
    reader.Nest();
    reader.Finish();

    if ( !secChecksumLoaded )
    {
        static bool warn = true;
        if ( warn )
        {
            Log::Err() << "Warning: No security checksum found. If this is an old recording, no worries; if it was made with DedCon 0.7 or later, it has been modified.\n";
            warn = false;
        }
    }

    assert( clientTagCount >= 0 );
    assert( seqID >= 0 );

    // load the tags
    serverTag.Load( s );

    while( --clientTagCount >= 0 )
    {
        // create shell in the list, append it to the end, and read it
        List< CompressedTag > * newTag = new List< CompressedTag >( clientTags );
        newTag->Load( s );

#ifdef COMPILE_DEDCON
        if( newTag->Size() > 0 )
        {
            TagReader reader( *newTag );
            std::string d = reader.Start();

            int objectId = -1;
            int teamId = -1;
            std::string t = "";

            while ( reader.AtomsLeft() )
            {
                // read key
                std::string key = reader.ReadKey();
                if( key == C_TAG_TYPE )
                {
                    t = reader.ReadString();
                }
                else if ( key == C_PLAYER_TEAM_ID )
                {
                    teamId = reader.ReadChar();
                }
                else if ( key == C_OBJECT_ID )
                {
                    objectId = reader.ReadInt();
                }
                else
                {
                    reader.ReadAny();
                }
            }
            reader.Nest();
            reader.Finish();

            if( d == "d" && t != "c1" && objectId >= 0 && teamId >= 0 )
            {
                Client::CheckOwnership( teamId, objectId );
            }
        }
#endif
    }

    if ( s.eof() )
    {
        Log::Err() << "Unexpected end of file while reading recording.\n";
        throw QuitException( -1 );
    }

    if ( !s.good() || s.fail() )
    {
        Log::Err() << "Unexpected error while reading recording.\n";
        throw QuitException( -1 );
    }

    secChecksum.Add( serverTag );
    secChecksum.Add( clientTags );
}

// recursive hepler function for sending the subsequences in the order they were received
// ( the list reverses that )
static void WriteSequence( List< CompressedTag > * run, int clientID, TagWriter & writer )
{
    if ( !run )
        return;

    // depth first for order reversal
    WriteSequence( run->Next(), clientID, writer );

    bool send = true;

    // check whitelist and blacklist
    if ( run->clientWhitelist.size() > 0 )
    {
        send = ( std::find( run->clientWhitelist.begin(), run->clientWhitelist.end(), clientID ) != run->clientWhitelist.end() );
    }

    send &= ( std::find( run->clientBlacklist.begin(), run->clientBlacklist.end(), clientID ) == run->clientBlacklist.end() );

    if ( send )
    {
        writer.WriteCompressed( *run );
    }
}


// writes the sequence to the passed writer as a complete tag
void Sequence::WriteDirect( TagWriter & writer, int lastSeqID, int clientID, bool resend )
{
    assert( lastSeqID > seqID );

    Collapse( clientTags, clientTagsBack );

    // open the tag as either C_TAG_RESEND or C_TAG.
    writer.Open( resend ? C_TAG_RESEND : C_TAG );

    if ( serverTag.Empty() )
    {
        // write dummy server tag
        writer.WriteString(C_TAG_TYPE,C_SEQUENCE);
        writer.WriteInt( C_SEQID_CURRENT, seqID );
        if ( !writer.IsNested() )
        {
            writer.WriteInt( C_SEQID_LAST, lastSeqID );
        }
    }
    else
    {
        // write the wrapping server tag
        TagParserRewriteSequence rewrite( writer, seqID, lastSeqID );
        TagReader reader( serverTag );
        rewrite.Parse( reader );
    }

    // write the conserved tags
    WriteSequence( clientTags, clientID, writer );
}

// writes the sequence to the passed writer as a complete tag
void Sequence::Write( TagWriter & writer, int lastSeqID, int clientID, bool resend )
{
    // write directly if possible
    if ( writer.GetState() == TagWriter::Raw )
        WriteDirect( writer, lastSeqID, clientID, resend );
    else
    {
        // nest otherwise
        TagWriter nested( writer );
        WriteDirect( nested, lastSeqID, clientID, resend );
    }
}

// inserts the contents of the writer into the sequence, assuming it comes from the client
void Sequence::InsertClient( AutoTagWriter const & writer )
{
    static const std::vector< int > whitelist, blacklist;
    InsertClient( writer, whitelist, blacklist );
}

//! inserts the contents of the writer into the sequence, assuming it comes from the client
void Sequence::InsertClient( AutoTagWriter const & writer, std::vector< int > const & whitelist, std::vector< int > const & blacklist, bool back )
{
    // create shell in the list, append it to the end
    List< CompressedTag > * newTag = new List< CompressedTag >( back ? clientTagsBack : clientTags );
    // and fill it
    writer.Compress( *newTag );
    newTag->clientWhitelist = whitelist;
    newTag->clientBlacklist = blacklist;

    size += writer.Size();
}

// inserts the contents of the writer into the sequence, assuming it comes from the server
void Sequence::InsertServer( AutoTagWriter const & writer )
{
    assert( serverTag.Empty() );

    // simply fill it
    writer.Compress( serverTag );

    size += writer.Size();
}

// inserts the contents of the tag into the sequence, assuming it comes from the server
void Sequence::InsertServer( CompressedTag const & tag )
{
    assert( serverTag.Empty() );
    serverTag = tag;
    size += tag.Size();
}

bool Sequence::HasServerTag() const
{
    return !serverTag.Empty();
}

bool Sequence::HasClientTag() const
{
    return (List< CompressedTag > *)clientTags || (List< CompressedTag > *)clientTagsBack;
}

void Sequence::AddSyncValue( Client * client, unsigned char syncValue_ )
{
    if ( !syncValueVote )
    {
        // vote was already cast, just compare the syncValue with the vote result
        client->OutOfSync( syncValue_ != syncValue, seqID );
    }
    else
    {
        // add the client's vote
        syncValueVote->FromClient( client, SyncValue::Single( syncValue_ ) );

        // and check whether a majority as been reached
        if ( syncValueVote->Majority( syncValue, seqID ) )
        {
            delete syncValueVote;
            syncValueVote = 0;
        }
    }
}

void Sequence::AddNoSyncValue( Client * client )
{
    if ( syncValueVote )
    {
        // add the client's vote
        syncValueVote->FromClient( client, SyncValue::Single() );
    }
}

bool Sequence::SyncValueMajority()
{
    // has a majority formed?
    if ( syncValueVote && syncValueVote->Majority( syncValue, seqID ) )
    {
        // yes, we can get rid of the vote
        delete syncValueVote;
        syncValueVote = 0;

        return true;
    }

    // return cached result
    return !syncValueVote;
}

