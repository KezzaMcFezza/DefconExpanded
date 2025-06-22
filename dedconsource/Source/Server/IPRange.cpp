/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#include "IPRange.h"
#include <assert.h>

// gets the bit's bit of address. Count start at the most significant bit.
static int GetBit( in_addr addr, int bit )
{
    return ( static_cast< unsigned int >( ntohl( addr.s_addr ) ) >> ( 31 - bit ) ) & 1;
}

IPSet::IPSet()
{
}

IPSet::~IPSet()
{
}

//! makes the specified range included in the set or not
void IPSet::Merge( IPRange const & range, UserInfo const & userInfo )
{
    assert( range.bits <= 32 );

    int levels = 0;
    IPSetNode * node = &root;
    while ( levels < range.bits )
    {
        node->Nodify();
        node = node->GetChild( GetBit( range.addr, levels ) );
        ++levels;
    }
    node->userInfo.Merge( userInfo );
}

//! clears the set
void IPSet::Clear()
{
    root.Clear();
    root.userInfo = UserInfo();
}

//! checks whether the specified IP is in the set 
void IPSet::Get( in_addr const & ip, UserInfo & userInfo ) const
{
    int levels = 0;
    IPSetNode const * node = &root;
    while ( node )
    {
        userInfo.Merge( node->userInfo );
        node = node->GetChild( GetBit( ip, levels ) );
        ++levels;
    }
}

IPSet::IPSetNode::IPSetNode()
{
    children[0] = children[1] = 0;
}

IPSet::IPSetNode::~IPSetNode()
{
    delete children[0];
    delete children[1];
    children[0] = children[1] = 0;
}

//! turn a leaf node into a real node by attaching two children and making them inherit
//! this node's tye
void IPSet::IPSetNode::Nodify()
{
    if ( !children[0] && !children[1] )
    {
        children[0] = new IPSetNode();
        children[1] = new IPSetNode();
    }
}

void IPSet::IPSetNode::Clear()
{
    // delete all leaves, they are no longer required
    delete children[0];
    delete children[1];
    children[0] = children[1] = 0;
}

IPSet::IPSetNode * IPSet::IPSetNode::GetChild( int index )
{
    assert( 0 <= index && index <= 1 );
    return children[index];
}

IPSet::IPSetNode const * IPSet::IPSetNode::GetChild( int index ) const
{
    assert( 0 <= index && index <= 1 );
    return children[index];
}

void IPSet::IPSetNode::MergeRecursive( UserInfo const & merge )
{
    userInfo.Merge( merge );
    for( int i = 1; i>0; --i)
    {
        IPSetNode * node = children[i];
        if ( node )
        {
            node->MergeRecursive( merge );
        }
    }
}
