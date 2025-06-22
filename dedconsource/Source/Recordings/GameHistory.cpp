/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "GameHistory.h"

#include "Lib/Codes.h"

#include "GameSettings/GameSettings.h"
#include "Sequence.h"
#include "TagReader.h"
#include "Server/Client.h"
#include "Lib/Globals.h"
#include "Main/Log.h"

#include <iostream>
#include <set>

#ifdef COMPILE_DEDCON
// Votes
void AllianceVoteHandler::AddVote( Client * client, int requesterTeamID, RequestType requestType, int requestData )
{
#ifdef DEBUG
    Log::Out() << "seqID " << gameHistory.GetTailSeqID() << ": Vote " << nextVoteID << " received: " << requesterTeamID << " wants to ";
    switch ( requestType )
    {
    case RequestType::Join:
        Log::Out() << "join color " << requestData << ".\n";
        break;
    case RequestType::Kick:
        Log::Out() << "kick player " << requestData << ".\n";
        break;
    case RequestType::Leave:
        Log::Out() << "leave the alliance.\n";
        break;
    default:
        Log::Out() << "DO SOMETHING UNKNOWN";
    }
#endif

    // find redundant votes
    for ( Vote const & vote : pendingVotes )
    {
        if ( vote.handled )
            continue;

        if ( vote.requesterTeamID == requesterTeamID &&
             vote.requestType == requestType &&
             vote.requestData == requestData )
        {
#if DEBUG
            Log::Out() << "Redundant vote ignored\n";
#endif
            return;
        }
    }

    pendingVotes.push_back( Vote{ nextVoteID, requesterTeamID, 60, requestType, requestData, false } );
    CastVote( client, requesterTeamID, nextVoteID, Verdict::Accept );

    ++nextVoteID;
}

void AllianceVoteHandler::CastVote( Client * client, int requesterTeamID, int voteID, Verdict verdict )
{
#ifdef DEBUG
    char const * v = "UNKNOWN";
    switch ( verdict )
    {
    case Verdict::Accept:
        v = "Accept";
        break;
    case Verdict::Deny:
        v = "Deny";
        break;
    case Verdict::Abstain:
        v = "Abstain";
    }

    Log::Out() << requesterTeamID << " votes on " << voteID << " with " << v << ", seqID " << gameHistory.GetTailSeqID() << ".\n";
#endif

    int seqID = gameHistory.GetTailSeqID();

    // find vote, cast
    for ( Vote & vote : pendingVotes )
    {
        if ( vote.voteID == voteID )
        {
            if ( vote.handled )
            {
#ifdef DEBUG
                Log::Out() << "Vote ignored, vote already handled.\n";
#endif
                return;
            }

            if ( verdict == Verdict::Accept )
                vote.inFavorTeamIDs.insert( requesterTeamID );
            else
                vote.inFavorTeamIDs.erase( requesterTeamID );

            if ( verdict == Verdict::Deny )
                vote.againstTeamIDs.insert( requesterTeamID );
            else
                vote.againstTeamIDs.erase( requesterTeamID );

            break;
        }
    }
}

void AllianceVoteHandler::HandleVotes()
{
    handleTimeout--;
    if ( handleTimeout > 0 )
        return;
    handleTimeout += 10;

    // expire votes
    while ( !pendingVotes.empty() &&
            pendingVotes.front().handled )
        pendingVotes.pop_front();

    for ( Vote & vote : pendingVotes )
    {
        vote.Handle();
    }
#ifdef DEBUG_X
    if ( !pendingVotes.empty() && !pendingVotes.front().handled )
        Log::Out() << "Next Vote Timeout: " << pendingVotes.front().timer << " seqID " << gameHistory.GetTailSeqID() << "\n";
#endif
}

bool AllianceVoteHandler::Vote::Eligible( int teamID ) const
{
    auto * superPower = SuperPower::GetTeam( teamID );
    if ( !superPower )
        return false;

    switch ( requestType )
    {
    case RequestType::Join:
        // JOIN: players f the joined alliance are allowed to vote
        return superPower->GetAllianceID() == requestData;

    case RequestType::Kick:
        // KICK: players of the same alliance as the kick candidate
        // are allowed, but not the kick candidate itself
        if ( requestData == teamID )
            return false;

        {
            auto * kickCandidate = SuperPower::GetTeam( requestData );
            if ( !kickCandidate )
                return false;
            return superPower->GetAllianceID() == kickCandidate->GetAllianceID();
        }

    case RequestType::Leave:
        // LEAVE: only the requester itself is allowed to vote
        return teamID == requesterTeamID;
    }

    return false;
}

int AllianceVoteHandler::Vote::EligibleCount() const
{
    int count = 0;
    for ( int i = 0; i < SuperPower::MaxSuperPowers; ++i )
    {
        if ( Eligible( i ) )
            ++count;
    }

    return count;
}

int AllianceVoteHandler::Vote::EligibleCount( std::set<int> const & teamIDs ) const
{
    int count = 0;
    for ( auto i : teamIDs )
    {
        if ( Eligible( i ) )
            ++count;
    }

    return count;
}

void AllianceVoteHandler::Vote::Deny()
{
#ifdef DEBUG
    if ( !handled )
        Log::Out() << "Vote " << voteID << " denied.\n";
#endif
    handled = true;
}

void AllianceVoteHandler::Vote::Accept()
{
#ifdef DEBUG
    if ( !handled )
        Log::Out() << "Vote " << voteID << " accepted.\n";
#endif
    handled = true;

    switch ( requestType )
    {
    case RequestType::Join: {
        auto * superPower = SuperPower::GetTeam( requesterTeamID );
        if ( superPower )
            superPower->SetAllianceID( requestData, "JOIN" );
        break;
    }
    case RequestType::Kick:
        // KICK
        LeaveAlliance( requestData, "KICK" );
        break;
    case RequestType::Leave:
        // LEAVE
        LeaveAlliance( requesterTeamID, "LEAVE" );
        break;
    default:
        break;
    }
}

void AllianceVoteHandler::Vote::Handle()
{
    if ( handled )
        return;

    timer--;
    const int totalEligible = EligibleCount();
    const int inFavor = EligibleCount( inFavorTeamIDs );
    const int against = EligibleCount( againstTeamIDs );

    int voteThreshold = totalEligible / 2;

    if ( inFavor > voteThreshold )
        Accept();
    else if ( against > voteThreshold || inFavor + against == totalEligible || timer <= 0 )
        Deny();
}

void AllianceVoteHandler::LeaveAlliance( int teamID, char const * reason )
{
    auto * superPower = SuperPower::GetTeam( teamID );
    if ( superPower )
    {
#ifdef DEBUG
        auto oldAllianceID = superPower->GetAllianceID();
#endif
        superPower->SetAllianceID( reason );
#ifdef DEBUG
        auto newAllianceID = superPower->GetAllianceID();
        Log::Out() << "Team " << teamID << "(" << superPower->GetName()
                   << ") leaves alliance " << oldAllianceID << "(" << reason
                   << "), new alliance " << newAllianceID << "\n";

#endif
    }
}
#endif

GameHistory::~GameHistory()
{
    while( sequences.size() )
    {
        delete sequences.front();
        sequences.pop_front();
    }
}

GameHistory::GameHistory()
{
    playback = false;
    checksumCheckBack = 0;
    seqIDStart = 0;
    seqIDEnd = 0x7fffffff;
    gameTicks = 0;
    lastEffectiveSeqID = 0;
    bumpRequested = false;
}

void GameHistory::Check()
{
    // check whether the bumping should start the server
    if ( !Playback() )
    {
        Client::CheckStart();
        
        // record game start
        if ( !serverInfo.started )
            seqIDStart = GetTailSeqID()+1;
    }
}

//! saves history to stream
void GameHistory::Save( std::ostream & s )
{
    // make sure the whole loaded history is inserted
    while ( loadedSequences.size() > 0 )
    {
        Bump();
    }

    // simply save all sequences
    for( iterator i = begin(); i != end(); ++i )
    {
        (*i)->Save( s );
    }
}

//! loads history from stream
void GameHistory::Load( std::istream & s )
{
    playback = true;

    // load until end of stream
    while ( s.good() && !s.fail() && !s.eof() )
    {
        Sequence * sequence = new Sequence( -1 );
        
        try
        {
            sequence->Load( s );
            if ( sequence->SeqID() >= 0 )
            {
                loadedSequences.push_back( sequence );

                
            }
            else
            {
                delete sequence;
            }
        }
        catch( std::ios::failure const & e )
        {
            delete sequence;
        }
    }
}

//! throws away the rest of the loaded history, leaving only the server tags
void GameHistory::DiscardLoad()
{
    while( loadedSequences.size() > 0 )
    {
        // get the first sequence
        Sequence * sequence = loadedSequences.front();
        loadedSequences.pop_front();

        // transfer the server tag, forget about the rest
        if ( !sequence->ServerTag().Empty() )
        {
            // log client quit times
            TagReader reader( sequence->ServerTag() );
            reader.Start();
            while ( reader.AtomsLeft() > 0 )
            {
                std::string key = reader.ReadKey();
                if ( key == C_TAG_TYPE )
                {
                    // only client quit messages are of interest
                    if ( reader.ReadString() != "sc" )
                    {
                        break;
                    }
                }
                else if ( key == C_PLAYER_ID )
                {
                    int id = reader.ReadInt();
                    Log::Out() << "Client " << id << " left at SeqID " << sequence->SeqID() << ".\n";
                }
                else
                {
                    reader.ReadAny();
                }
            }
            reader.Obsolete();

            Sequence * s = ForServer();
            s->InsertServer( sequence->ServerTag() );
        }

        delete sequence;
    }
}

// adds a sequence to the list
void GameHistory::Append( Sequence * sequence )
{
    if ( !sequence )
        return;

    // add it to the global list
    sequences.push_back( sequence );

    // add it to each client's non-ack list
    for ( Client::iterator c = Client::GetFirstClient(); c; ++c )
    {
        if ( c->notYetAcknowledged.size() > 0 )
            assert( c->notYetAcknowledged.back().sequence->SeqID() < sequence->SeqID() );

        c->notYetAcknowledged.push_back( SequenceAck( sequence ) );
    }

    /*
    // for every added sequence, check up do two checksums
    checksumCheckBack++;
    CheckChecksums() && 
    CheckChecksums();
    */
}

// bumps the game clock
void BumpTime( int & gameTicks )
{
    // determine the game speed manipulator
    if ( !serverInfo.started )
    {
        return;
    }

#ifdef COMPILE_DEDWINIA
    // game time starts as soon as all players are ready for 30 ticks
    if ( !serverInfo.unstoppable || serverInfo.everyoneReadySince + 30 > gameHistory.GetTailSeqID() )
    {
        return;
    }
#endif    

    int speed = 1;
#ifdef COMPILE_DEDCON
    switch ( settings.gameSpeed )
    {
    case 0:
        speed = Client::TimeBudget( 0 );
        break;
    case 1:
        speed = 1;
        break;
    case 2:
        speed = 5;
        break;
    case 3:
        speed = 10;
        break;
    case 4:
        speed = 20;
        break;
    }
#endif

    gameTicks += speed;
}

// returns the last sequence
Sequence * GameHistory::GetTail( int seqID )
{
    bool runCheck = false;

    // bump the sequence ID up to the desired spot
    int lastSeqID = -1;
    if ( sequences.size() > 0 )
    {
        lastSeqID = sequences.back()->SeqID();
    }
    while ( lastSeqID < seqID || sequences.size() == 0 || sequences.back()->Size() > settings.maxPacketSize )
    {
        bumpRequested = false;

        ++lastSeqID;
        Sequence * toInsert = 0;

        // check if there is a prepared loaded sequence
        if ( loadedSequences.size() > 0 )
        {
            toInsert = loadedSequences.front();
            if ( toInsert->SeqID() <= lastSeqID )
            {
                // sequence fits, fine, take it
                loadedSequences.pop_front();
                
                if ( toInsert->GetComment().size() > 0 )
                {
                    Log::Out() << toInsert->GetComment() << "\n";
                }

                if ( toInsert->SeqID() < 0 )
                {
                    toInsert = 0;
                }
            }
            else
            {
                // sequence toes not fit, insert own sequence
                toInsert = 0;
            }
        }

        // create new sequence
        if ( !toInsert )
        {
            toInsert = new Sequence( lastSeqID );
        }

        // and update the game time
        if ( !Playback() )
        {
            BumpTime( gameTicks );

#ifdef COMPILE_DEDCON
            if ( serverInfo.started && !serverInfo.ended )
                allianceVoteHandler.HandleVotes();
#endif
        }

        // and append it
        Append( toInsert );

        runCheck = true;
    }

    // get the last sequence
    Sequence * insertTo = sequences.back();
    assert( insertTo->SeqID() >= seqID );

    // run checks
    if ( runCheck )
        Check();

    // and return it
    return insertTo;
}

// insert a tag from the server
Sequence * GameHistory::ForServer(int seqID )
{
    Sequence * tail = GetTail( seqID );
    while ( tail->HasServerTag() || tail->HasClientTag() )
    {
        tail = GetTail( ++seqID );
    }

    return tail;
}

// insert a tag from the server
Sequence * GameHistory::ForServer()
{
    return ForServer( gameClock.Current() );
}

//! inserts a tag from the server
Sequence *  GameHistory::AppendServer( AutoTagWriter const & writer )
{
    Sequence * s = ForServer();
    s->InsertServer( writer );
    return s;
}

// insert a tag from a client
Sequence * GameHistory::ForClient( int seqID )
{
    Sequence * tail = GetTail( seqID );
    while ( tail->HasServerTag() )
    {
        tail = GetTail( ++seqID );
    }

    return tail;
}

// insert a tag from a client
Sequence * GameHistory::ForClient()
{
    return ForClient( GetTailSeqID() );
}

//! inserts a tag from a client
Sequence * GameHistory::AppendClient( AutoTagWriter const & writer )
{
    Sequence * s = ForClient();
    s->InsertClient( writer );
    return s;
}

int GameHistory::GetTailSeqID() const
{
    if ( sequences.size() == 0 )
        return -1;
    else
        return sequences.back()->SeqID();
}

void GameHistory::Bump()
{
    GetTail( GetTailSeqID() + 1 );
    Check();
}

//! checks the gamestate checksums
bool GameHistory::CheckChecksums()
{
    /*
    if ( (int)sequences.size() < checksumCheckBack || sequences.size() == 0 )
    {
        checksumCheckBack--;
        return false;
    }

    // test this sequence
    Sequence & sequence = **(sequences.end() - checksumCheckBack);

    if ( !sequence.ChecksumMajority() )
    {
        // try again next time
        return false;
    }

    // advance
    checksumCheckBack--;
    */
    
    return true;
}

//! returns the last SeqID in the queue as far as the clients are concerned
int GameHistory::GetEffectiveTailSeqID() const
{
    return lastEffectiveSeqID;
}

//! sets the last SeqID in the queue as far as the clients are concerned
void GameHistory::SetEffectiveTailSeqID( int seqID )
{
    lastEffectiveSeqID = seqID ;
}

//! executes bump if one was requested
void GameHistory::BumpIfRequested()
{
    if ( bumpRequested )
    {
        Bump();
    }

    // bump the last effective seqID at most twice
    int tailSeqID = GetTailSeqID();
    if ( tailSeqID > lastEffectiveSeqID )
    {
        lastEffectiveSeqID++;
    }
    if ( tailSeqID > lastEffectiveSeqID )
    {
        lastEffectiveSeqID++;
    }

#ifdef COMPILE_DEDCON
    // update player time budget
    if ( !Playback() )
    {
        static int lastTimeBudget = 0;
        if ( settings.gameSpeed == 0 && lastTimeBudget < tailSeqID )
        {
            Client::TimeBudget( ( tailSeqID - lastTimeBudget ) * .1 );
        }
        lastTimeBudget = tailSeqID;
    }
#endif
}

GameHistory gameHistory;
