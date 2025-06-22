/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "SyncValue.h"

#include <stdlib.h>
#include <string.h>

#include "Server/Client.h"

// see if a new set of votes is better than an old set of votes (counting total votes and votes from in-sync clients)
static inline bool VotesAreBetter( int votes, int votesInSync, int oldVotes, int oldVotesInSync )
{
    if ( votes > oldVotes )
    {
        return true;
    }
    if ( votes < oldVotes )
    {
        return false;
    }
    return votesInSync > oldVotesInSync;
}
    
//! see if enough votes have been cast to determine the right checksum
bool SyncValue::Majority( unsigned char & checksum, int seqID, bool force ) const
{
    // count the clients
    int clients = 0;
    for ( Client::const_iterator c = Client::GetFirstClient(); c; ++c )
    {
        // only count clients that come at least close to being current
        if ( c->notYetAcknowledged.size() < 1000 )
            clients++;
    }
    
    // can a majority be reached at all?
    if ( clients >= (int)map.size()*2 && !force )
        return false;

    // count the number of total votes and the number of votes from clients
    // that are not yet marked as out of sync
    unsigned char numberOfVotes[256], numberOfVotesInSync[256];
    memset( numberOfVotes, 0, sizeof(numberOfVotes) );
    memset( numberOfVotesInSync, 0, sizeof(numberOfVotesInSync) );
    
    // determine the checksum most clients voted for
    int maxVotes = 0, maxVotesInSync = 0;
    int bestSyncValue = -1;
    for ( Map::const_iterator i = map.begin(); i != map.end(); ++i )
    {
        Single const & checksum = (*i).second;
        if ( checksum.set )
        {
            // count number of votes
            numberOfVotes[checksum.val]++;
            if ( !(*i).first->outOfSync )
            {
                numberOfVotesInSync[checksum.val]++;
            }

            if ( VotesAreBetter( numberOfVotes[checksum.val], numberOfVotesInSync[checksum.val], maxVotes, maxVotesInSync ) )
            {
                maxVotes = numberOfVotes[checksum.val];
                maxVotesInSync = numberOfVotesInSync[checksum.val];
                bestSyncValue = checksum.val;
            }
        }
        else
        {
            // no vote, reduce number of clients
            clients -= 2;
        }
    }

    // find second largest vote (needed to determine when the voting process should
    // be over)
    int secondLargestVote = 0;
    int secondLargestVoteInSync = 0;
    for ( Map::const_iterator i = map.begin(); i != map.end(); ++i )
    {
        Single const & checksum = (*i).second;
        if ( checksum.set )
        {
            if ( checksum.val != bestSyncValue && VotesAreBetter( numberOfVotes[checksum.val], numberOfVotesInSync[checksum.val], secondLargestVote, secondLargestVoteInSync ) )
            {
                secondLargestVote = numberOfVotes[checksum.val];
                secondLargestVoteInSync = numberOfVotesInSync[checksum.val];
            }
        }
    }
    
    if ( maxVotes == 0 )
    {
        // no votes yet
        return false;
    }
    else if ( clients == maxVotes * 2 && secondLargestVote == maxVotes && secondLargestVoteInSync == maxVotesInSync && !force )
    {
        // special case: at least two strong groups of equal size and all have voted.
        // tell everyone they are out of sync. No group has the majority.
        for ( Map::const_iterator i = map.begin(); i != map.end(); ++i )
        {
            (*i).first->OutOfSync( true, seqID );
        }
        
        // but don't decide who is right.
        return false;
    }
    else if ( force ? ( maxVotes > secondLargestVote ) : ( clients >= 2 && maxVotes > clients - (int)map.size() + secondLargestVote ) )
    {
        // we have a majority that can't be overruled
        checksum = bestSyncValue;
        
        // tell the others they are wrong.
        for ( Map::const_iterator i = map.begin(); i != map.end(); ++i )
        {
            if ( (*i).second.set )
            {
                // static int count = 100;

                bool outOfSync = (*i).second.val != checksum; // || ( --count < 0 && count > -100 );
                (*i).first->OutOfSync( outOfSync, seqID );
            }
        }
        
        return true;
    }
    
    // not enough votes yet
    return false;
}
