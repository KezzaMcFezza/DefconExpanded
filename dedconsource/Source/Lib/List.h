/*
 *  * DedCon: Dedicated Server for Defcon
 *  *
 *  * Copyright (C) 2007 Manuel Moos
 *  *
 *  */

#ifndef DEDCON_List_INCLUDED
#define DEDCON_List_INCLUDED

#include "Misc.h"

// invasive linked list items
template< class T > class List: public T
{
public:
    template< class ARG > List( ARG & arg, List * * anchor_ )
    : T(arg), anchor(0), next(0)
    {
        if ( anchor_ )
            Insert( *anchor_ );
    }

    List( List * * anchor_ = 0 )
    : anchor(0), next(0)
    {
        if ( anchor_ )
            Insert( *anchor_ );
    }

    ~List()
    {
        if ( anchor )
            Remove();
    }

    void Insert( List * & anchor_ )
    {
        assert( !anchor );

        anchor = &anchor_;
        next = *anchor;
        *anchor = this;
        if ( next )
            next->anchor = &next;
    }

    void Remove()
    {
        assert( anchor );
        *anchor = next;
        if ( next )
            next->anchor = anchor;
        next = 0;
        anchor = 0;
    }

    bool IsInList() const
    {
        return anchor;
    }
    
    void InsertSafely( List * & anchor_ )
    {
        if ( anchor != &anchor_ )
        {
            if ( IsInList() )
                Remove();
            Insert( anchor_ );
        }
    }

    static void Erase( List * & anchor )
    {
        while( anchor )
            delete anchor;
    }

    List * Next()
    {
        return next;
    }

    List const * Next() const
    {
        return next;
    }
private:
    List **anchor;
    List *next;
};

// base for linked lists
template< class T > class ListBase
{
public:
    class const_iterator
    {
    protected:
        List<T> const * current;
        List<T> const * next;
    public:
        const_iterator( List<T> const * start )
        {
            current = start;
            if ( current )
                next = current->Next();
            else
                next = 0;
        }

        List<T> const & operator * ()
        {
            assert( current );
            return *current;
        }

        List<T> const * operator -> ()
        {
            assert( current );
            return current;
        }

        const_iterator & operator++()
        {
            current = next;
            if ( current )
                next = current->Next();

            return *this;
        }

        operator bool()
        {
            return current;
        }

        bool operator !()
        {
            return !current;
        }
    private:
        const_iterator & operator++( int ); // use ++iterator instead.
    };

    class iterator: public const_iterator
    {
    public:
        iterator( List<T> * start )
        : const_iterator( start ){};

        List<T> & operator * ()
        {
            assert( this->current );
            return *const_cast< List<T> * >( this->current );
        }

        List<T> * operator -> ()
        {
            assert( this->current );
            return const_cast< List<T> * >( this->current );
        }

        iterator & operator++()
        {
            const_iterator::operator ++();

            return *this;
        }
    };

    ListBase( bool owned_ = true ): anchor(0), owned( owned_ ){}
    ~ListBase(){ if ( owned ) List<T>::Erase( anchor ); }

    const_iterator begin() const
    {
        return const_iterator( anchor );
    }

    const_iterator end() const
    {
        return const_iterator( 0 );
    }

    iterator begin()
    {
        return iterator( anchor );
    }

    iterator end()
    {
        return iterator( 0 );
    }

    operator List<T> * *()
    {
        return &anchor;
    }

    operator List<T> * &()
    {
        return anchor;
    }

    operator List<T> * () const
    {
        return anchor;
    }

    operator bool()
    {
        return anchor;
    }

    void Clear()
    {
        List< T >::Erase( anchor );
    }
private:
    ListBase( const ListBase & );
    ListBase & operator = ( const ListBase & );

    List<T> * anchor;

    bool owned;
};

#endif // DEDCON_List_INCLUDED
