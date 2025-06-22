/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_GameHistory_INCLUDED
#define DEDCON_GameHistory_INCLUDED

#include "Lib/Forward.h"
#include <deque>
#include <iosfwd>
#include <set>

#ifdef COMPILE_DEDCON
// Votes
class AllianceVoteHandler
{
public:
    enum RequestType
    {
        Join = 1,
        Kick = 2,
        Leave = 3,
    };

    enum Verdict
    {
        Accept = 1,
        Deny = 2,
        Abstain = 3,
    };

    void AddVote( Client * client, int requesterTeamID, RequestType requestType, int requestData );
    void CastVote( Client * client, int requesterTeamID, int voteID, Verdict verdict );

    void HandleVotes();

    AllianceVoteHandler(): handleTimeout{ 9 }, nextVoteID{ 0 } {}

private:
    // ticks until next vote handling
    int handleTimeout;

    // runnung ID for votes; each vote gets a new one
    int nextVoteID;

    struct Vote
    {
        int voteID;
        int requesterTeamID;
        int timer;
        RequestType requestType;
        int requestData;
        bool handled;

        // voters
        std::set<int> inFavorTeamIDs;
        std::set<int> againstTeamIDs;

        bool Eligible( int teamID ) const;

        int EligibleCount() const;
        int EligibleCount( std::set<int> const & teamIDs ) const;

        void Deny();
        void Accept();

        void Handle();
    };

    std::deque<Vote> pendingVotes;

    static void LeaveAlliance( int teamID, char const * reason );
};
#endif

//! the total game history
class GameHistory
{
public:
    typedef std::deque< Sequence * > Sequences;     //!< queue of sequences
    typedef Sequences::const_iterator iterator;     //!< queue iterator

#ifdef COMPILE_DEDCON
    AllianceVoteHandler allianceVoteHandler;
#endif

    iterator begin()const { return sequences.begin(); } //!< begin of sequence queue
    iterator end() const { return sequences.end(); }    //!< end of sequence queue

    ~GameHistory();
    GameHistory();

    //! saves history to stream
    void Save( std::ostream & s );

    //! loads history from stream
    void Load( std::istream & s );

    //! throws away the rest of the loaded history, leaving only the server tags
    void DiscardLoad();

    //! adds a sequence to the list
    void Append( Sequence * sequence );

    //! does checks that need to be done periodically
    void Check();
    
    //! returns the last sequence and makes sure its seqID is at least the one that is passed
    Sequence * GetTail( int seqID );

    //! returns a sequence ready to take a tag from the server, makes sure its seqID is at least the one that is passed
    Sequence *  ForServer( int seqID );

    //! returns a sequence ready to take a tag from the server
    Sequence *  ForServer();

    //! inserts a tag from the server
    Sequence *  AppendServer( AutoTagWriter const & writer );

    //! returns a sequence ready to take a tag from the client, makes sure its seqID is at least the one that is passed
    Sequence *  ForClient( int seqID );

    //! returns a sequence ready to take a tag from the client, makes sure its seqID is at least the one that is passed
    Sequence *  ForClient();

    //! inserts a tag from a client
    Sequence *  AppendClient( AutoTagWriter const & writer );

    //! returns the last SeqID in the queue
    int GetTailSeqID() const;

    //! returns the last SeqID in the queue as far as the clients are concerned
    int GetEffectiveTailSeqID() const;

    //! sets the last SeqID in the queue as far as the clients are concerned
    void SetEffectiveTailSeqID( int seqID );

    //! returns the current GameTime in units of 0.1 seconds
    int GameTime(){ return gameTicks; }

    //! bumps the last SeqID by one by appending an empty sequence
    void Bump();

    //! checks the gamestate checksums
    bool CheckChecksums();

    //! requests a bump at the next convenience
    void RequestBump()
    {
        bumpRequested = true;
    }

    //! executes bump if one was requested
    void BumpIfRequested();

    //! returns whether a recording is currently being played back
    bool Playback() const { return playback; }

    //! returns whether the playback has reached the end
    bool PlaybackEnded() const { return loadedSequences.size() == 0; }

    //! the SeqID where the game starts
    int GetSeqIDStart() const{ return seqIDStart; }
    void SetSeqIDStart( int start ) { seqIDStart = start; }

    //! the SeqID where the game ends
    int GetSeqIDEnd() const{ return seqIDEnd; }
    void SetSeqIDEnd( int start ) { seqIDEnd = start; }
private:
    int gameTicks;                     //!< the game time in units of 0.1 seconds

    int seqIDStart;                    //!< the seqID at which the game started
    int seqIDEnd;                      //!< the seqID at which the game ended

    bool playback;                     //!< returns whether this is a playback

    bool bumpRequested;                //!< whether a bump was requested

    int lastEffectiveSeqID;            //!< the last seqID as reported to the clients

    Sequences sequences;               //!< the sequence queue
    Sequences loadedSequences;         //!< the sequences that were loaded from a file
    int checksumCheckBack;             //!< the distance to the back of the queue we're checking the checksums
};

extern GameHistory gameHistory;

#endif // DEDCON_GameHistory_INCLUDED
