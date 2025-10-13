#ifndef _included_fastlist_h
#define _included_fastlist_h

/*
 *  Fast List container class
 *
 *  Essentially a linked list, automatically grows and shrinks
 *  Indexes of elements are not maintained
 *
 *  Unlike LList, this class has immediate fast access to data at any index
 *  But slower insertion and deletion.
 *  However insertion at end index (standard PutData behaviour) is much faster.
 *
 *  Use CopyableFastList if you need the list to be copyable, e.g. as a class
 *  member when class instances will be stored in an array.  Remember that
 *  copying a list is slow!
 */


template <class T>
class FastList
{
protected:
    T           *m_array;
    int         m_arraySize;
    int         m_numItems;

    void        EnsureSpace ();
    void        MoveRight   ( int firstIndex );
    void        MoveLeft    ( int firstIndex );

public:
    FastList();
    ~FastList();

    void        Copy( const FastList &source );

    int         PutData			( const T &newdata );               // fast. Adds in data at the end.   Returns index assigned.
    int 		PutDataAtEnd	( const T &newdata );               // fast.                            Returns index assigned.
    int     	PutDataAtStart	( const T &newdata );	            // slow                             Returns index assigned.
    int     	PutDataAtIndex	( const T &newdata, int index );    // slow                             Returns index assigned.

    T	        GetData			( int index ) const;	            // Fast
    T          &GetDataRef  	( int index ) const;	            // Fast
    T          *GetPointer		( int index ) const;                // Fast
    void		RemoveData		( int index );			            // slow
    void        RemoveDataWithSwap( int index );                    // Fast, swaps index with final element
                                                                    // before removing it.
    int			FindData		( const T &data ) const;		    // -1 means 'not found'

    int	        Size			() const;				           // Returns the number of elements
    void        EnsureSpace     ( int capacity );                  // Ensures backing array has given capacity.
    int         Capacity        () const;                          // Returns allocated number of elements.
    void        Resize          ( int size );                      // Grow or shrink array.
    bool        ValidIndex		( int index ) const;

    void        Append          ( const FastList<T> & other );
    void        Swap            ( FastList<T> & other );
    void		Empty			();						            // Resets the array to empty
    void		EmptyAndDelete	();						            // As above, deletes all data as well

    T           &operator []    (int index) const;
    bool        operator ==     ( FastList<T> const &_other ) const;
    bool        operator !=     ( FastList<T> const &_other ) const;

private:
    FastList( const FastList & );  // = delete
    FastList &operator = ( const FastList & );  // = delete
};


template <class T>
class CopyableFastList : public FastList<T>
{
public:
    CopyableFastList();
    CopyableFastList( const CopyableFastList &source );
    CopyableFastList &operator = ( const CopyableFastList &source );
};


// Copies matching items to the end of the destination list.
template<class T, class P>
void CopyMatching( const FastList<T> &source, FastList<T> &destination, P predicate );

// Moves matching items from source list to the end of destination list in one pass.
// (Much more efficient than calling RemoveData repeatedly).
template<class T, class P>
void MoveMatching ( FastList<T> &source, FastList<T> &destination, P predicate );

// Moves matching items from source list to front of destination list in one pass.
// (Much more efficient than calling RemoveData repeatedly).
template<class T, class P>
void MoveMatchingToFront ( FastList<T> &source, FastList<T> &destination, P predicate );

template<class T, class P>
int CountMatching( FastList<T> const &source, P predicate );

// RemoveMatching is only safe to use when T is a not a pointer to owned data, because
// operator delete will not be called on the removed items.
template<class T, class P>
void RemoveMatching( FastList<T> const &source, P predicate );



#include "fastlist.cpp"

#endif

