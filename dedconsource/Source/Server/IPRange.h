/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_IPRange_INCLUDED
#define DEDCON_IPRange_INCLUDED

#include "Network/Socket.h"
#include "Lib/UserInfo.h"

//! a single IP range
class IPRange
{
public:
    in_addr addr;  //<! the base address
    char    bits;  //<! the number of significant bits
};

//! bit ordered tree of IP addresses
class IPSet
{
public:
    IPSet();
    ~IPSet();

    //! sets the user info for the specified range
    void Merge( IPRange const & range, UserInfo const & userInfo );

    //! clears the set
    void Clear();

    //! fills the user info for the specified IP (merging all infos on the way)
    void Get( in_addr const & ip, UserInfo & userInfo ) const;
private:
    IPSet( IPSet const & );
    IPSet & operator = ( IPSet const & );

    class IPSetNode
    {
    public:
        IPSetNode();
        ~IPSetNode();

        //! turn a leaf node into a real node by attaching two children and making them inherit
        //! this node's type
        void Nodify();
        
        //! clears child nodes
        void Clear();

        IPSetNode * GetChild( int index );
        IPSetNode const * GetChild( int index ) const;

        void MergeRecursive( UserInfo const & merge );

        UserInfo userInfo;
    private:

        IPSetNode * children[2];
    };

    IPSetNode root;
};

#endif // DEDCON_IPRange_INCLUDED
