/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_Sequence_INCLUDED
#define DEDCON_Sequence_INCLUDED

#include "Lib/List.h"
#include "Lib/Forward.h"
#include "CompressedTag.h"

//! sequences. A sequence contains the compressed events of a tick.
class Sequence
{
public:
    Sequence( AutoTagWriter const & writer, int seqID_ );
    Sequence( int seqID_ );
    ~Sequence();
    
    //! saves sequence to stream
    void Save( std::ostream & s ) const;

    //! loads sequence from stream
    void Load( std::istream & s );

    //! writes the sequence to the passed writer as a complete tag
    void WriteDirect( TagWriter & writer, int lastSeqID, int clientID, bool resend );

    //! writes the sequence to the passed writer as a complete tag, nested if there is no room otherwise
    void Write( TagWriter & writer, int lastSeqID, int clientID, bool resend );

    //! inserts the contents of the writer into the sequence, assuming it comes from the client
    void InsertClient( AutoTagWriter const & writer );

    //! inserts the contents of the writer into the sequence, assuming it comes from the client
    void InsertClient( AutoTagWriter const & writer, std::vector< int > const & whitelist, std::vector< int > const & blacklist, bool back = false );

    //! inserts the contents of the writer into the sequence, assuming it comes from the server
    void InsertServer( AutoTagWriter const & writer );

    //! inserts the contents of the compressed tag into the sequence, assuming it comes from the server
    void InsertServer( CompressedTag const & serverTag );
 
    bool HasServerTag() const; //!< true of a server message has been stored
    bool HasClientTag() const; //!< true of a client message has been stored

    void AddSyncValue( Client * client, unsigned char syncValue ); //!< the specified client thinks that the passed syncValue is the right one
    void AddNoSyncValue( Client * client ); //!< the specified client doesn't know about a syncValue
    bool SyncValueMajority();               //!< checks whether there is a syncValue majority

    //! the SeqID of the sequence
    int SeqID() const { return seqID; }

    //! returns the size of the data in bytes (this is only an approximation and does not take rewriting and wrapping into account)
    int Size() const{ return size; }

    CompressedTag const & ServerTag() const { return serverTag; }
    List< CompressedTag > const * ClientTag() const { return clientTags; }

    void SetComment( std::string const c ){ comment = c; }
    std::string const & GetComment(){ return comment; }
private:
    CompressedTag serverTag;               //!< the tag from the server
    ListBase< CompressedTag > clientTags;  //!< the tags as sent by the clients
    ListBase< CompressedTag > clientTagsBack;  //!< the tags as sent by the clients, ones that should stay at the back
    std::string comment;                   //!< comment for later
    int size;                              //!< the accumulated size of the sequence
    int seqID;                             //!< the sequence ID
    mutable SyncValue * syncValueVote;     //!< the SycnValue voting process
    mutable unsigned char syncValue;       //!< if the vote is finished, the gamestate SyncValue
};

#endif // DEDCON_Sequence_INCLUDED
