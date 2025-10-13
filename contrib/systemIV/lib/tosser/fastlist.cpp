
#include "lib/math/math_utils.h"
#include "lib/debug_utils.h"


template <class T>
FastList<T>::FastList()
:   m_array(NULL),
    m_arraySize(0),
    m_numItems(0)
{
}


template <class T>
FastList<T>::~FastList()
{
    if( m_array ) delete [] m_array;
}


template <class T>
void FastList<T>::Copy( const FastList<T> &source )
{
    Empty();

    if( source.m_numItems == 0 ) return;

    m_array = new T[source.m_numItems];

    for( int i = 0; i < source.m_numItems; ++i )
    {
        m_array[i] = source.m_array[i];
    }
    
    m_arraySize = source.m_numItems;
    m_numItems = source.m_numItems;
}


template <class T>
void FastList<T>::EnsureSpace()
{
    if( m_arraySize > m_numItems ) return;

    int newArraySize = m_arraySize;
    newArraySize = max( newArraySize, 10 );
    while( newArraySize <= m_numItems ) newArraySize *= 2;
    EnsureSpace( newArraySize );
}


template <class T>
void FastList<T>::EnsureSpace( int newArraySize )
{
    if( newArraySize <= m_arraySize ) return;

    T *newArray = new T[newArraySize];
        
    if( m_array )
    {            
        for( int i = 0; i < m_arraySize; ++i )
        {
            newArray[i] = m_array[i];
        }
            
        delete [] m_array;
    }

    m_array = newArray;
    m_arraySize = newArraySize;
}


template <class T>
void FastList<T>::MoveRight( int firstIndex )
{
    if( !m_array ) return;
    
    for( int i = m_numItems; i > firstIndex; --i )
    {
        AppAssert( i >= 0 && i < m_arraySize );
        m_array[i] = m_array[i-1];
    }
}


template <class T>
void FastList<T>::MoveLeft( int firstIndex )
{
    if( !m_array ) return;
    
    for( int i = firstIndex; i < m_numItems-1; ++i )
    {
        AppAssert( i >= 0 && i < m_arraySize );
        m_array[i] = m_array[i+1];
    }
}


template <class T>
int FastList<T>::PutData( const T &newdata )
{
    return PutDataAtIndex(newdata, m_numItems);
}


template <class T>
int FastList<T>::PutDataAtEnd( const T &newdata )
{
    return PutDataAtIndex( newdata, m_numItems );
}


template <class T>
int FastList<T>::PutDataAtStart( const T &newdata )
{
    return PutDataAtIndex( newdata, 0 );
}


template <class T>
int FastList<T>::PutDataAtIndex( const T &newdata, int index )
{
    //AppDebugOut( "Putting data at index %d\n", index );
    
    EnsureSpace();

    MoveRight(index);
    ++m_numItems;

    m_array[index] = newdata;

    return index;
}


template <class T>
inline
T FastList<T>::GetData( int index ) const
{
    AppDebugAssert(index >= 0);
    AppDebugAssert(index < m_numItems);

    return m_array[index];
}


template <class T>
inline
T &FastList<T>::GetDataRef( int index ) const
{
    AppDebugAssert(index >= 0);
    AppDebugAssert(index < m_numItems);

    return m_array[index];
}


template <class T>
T *FastList<T>::GetPointer( int index ) const
{
    AppDebugAssert(index >= 0);
    AppDebugAssert(index < m_numItems);

    return &m_array[index];
}


template <class T>
int	FastList<T>::FindData( const T &data ) const
{
    for( int i = 0; i < m_numItems; ++i )
    {
        if( m_array[i] == data )
        {
            return i;
        }
    }

    return -1;
}


template <class T>
void FastList<T>::RemoveData( int index )
{
    AppAssert(0 <= index && index < m_numItems);

    MoveLeft(index);
    --m_numItems;
}


template <class T>
void FastList<T>::RemoveDataWithSwap( int index )
{
    AppAssert(0 <= index && index < m_numItems);

    int lastIndex = m_numItems - 1;
    if( index != lastIndex )
    {
        m_array[index] = m_array[lastIndex];
    }

    Resize( lastIndex );
}


template <class T>
inline
int FastList<T>::Size() const
{
    return m_numItems;
}


template <class T>
inline
int FastList<T>::Capacity() const
{
    return m_arraySize;
}


template <class T>
inline
void FastList<T>::Resize( int size )
{
    AppDebugAssert(size >= 0);

    m_numItems = size;
    EnsureSpace();
}


template <class T>
bool FastList<T>::ValidIndex( int index ) const
{
    return( index >= 0 && index < m_numItems );
}


template <class T>
void FastList<T>::Empty()
{
    if( m_array )
    {
        delete [] m_array;
        m_array = NULL;
    }
    
    m_arraySize = 0;
    m_numItems = 0;
}


template <class T>
void FastList<T>::Append( const FastList<T> & other )
{
    if( !other.Size() ) return;

    int oldSize = Size();
    Resize( oldSize + other.Size() );
    for( int i = 0; i < other.Size(); ++i )
    {
        m_array[oldSize + i] = other.m_array[i];
    }
}


template <class T>
void FastList<T>::Swap( FastList<T> & other )
{
    std::swap( m_array, other.m_array );
    std::swap( m_arraySize, other.m_arraySize );
    std::swap( m_numItems, other.m_numItems );
}


template <class T>
void FastList<T>::EmptyAndDelete()
{
    int numItems = m_numItems;
    m_arraySize = 0;
    m_numItems = 0;

    if( m_array )
    {
        for( int i = 0; i < numItems; ++i )
        {
            T item = m_array[i];
            delete item;
        }
    }

    Empty();
}

template <class T>
inline
T &FastList<T>::operator [](int index) const
{
    return GetDataRef(index);
}

template <class T>
bool FastList<T>::operator ==( FastList<T> const &_other ) const
{
    // Check the lists are the same length
    if( Size() != _other.Size() )   return false;

    // Then check ordering of the list
    for( int i = 0; i < m_numItems; i++ )
    {
        if( m_array[ i ] != _other[ i ] )
        {
            return false;
        }
    }

    return true;
}

template <class T>
bool FastList<T>::operator !=( FastList<T> const &_other ) const
{
    return !( *this == _other );
}


template <class T>
CopyableFastList<T>::CopyableFastList()
{
}


template <class T>
CopyableFastList<T>::CopyableFastList( const CopyableFastList<T> &source )
{
    this->Copy( source );
}


template <class T>
CopyableFastList<T> &CopyableFastList<T>::operator = ( const CopyableFastList<T> &source )
{
    this->Copy( source );
    return *this;
}


template<class T, class P>
void CopyMatching( const FastList<T> &source, FastList<T> &destination, P predicate )
{
    for( int i = 0; i < source.Size(); ++i )
    {
        const T& x = source[i];
        if( predicate( x ) )
        {
            destination.PutData( x );
        }
    }
}


template<class T, class P>
void MoveMatching( FastList<T> &source, FastList<T> &destination, P predicate )
{
    int writeIndex = 0;
    for( int readIndex = 0; readIndex < source.Size(); ++readIndex )
    {
        T& x = source[readIndex];
        if( predicate( x ) )
        {
            destination.PutData( x );
        }
        else
        {
            if( readIndex != writeIndex )
            {
                source[writeIndex] = x;
            }
            ++writeIndex;
        }
    }

    if( writeIndex != source.Size() ) source.Resize( writeIndex );
}


template<class T, class P>
void MoveMatchingToFront( FastList<T> &source, FastList<T> &destination, P predicate )
{
    FastList<T> matched;

    MoveMatching( source, matched, predicate );
    if( matched.Size() )
    {
        matched.Swap( destination );
        for( int i = 0; i < matched.Size(); ++i )
        {
            destination.PutData( matched[i] );
        }
    }
}


template<class T, class P>
int CountMatching( FastList<T> const &source, P predicate )
{
    int count = 0;
    for( int readIndex = 0; readIndex < source.Size(); ++readIndex )
    {
        if( predicate( source[readIndex] ) ) ++count;
    }
    return count;
}


template<class T, class P>
void RemoveMatching( FastList<T> &source, P predicate )
{
    int writeIndex = 0;
    for( int readIndex = 0; readIndex < source.Size(); ++readIndex )
    {
        T& x = source[readIndex];
        if( !predicate( x ) )
        {
            if( readIndex != writeIndex )
            {
                source[writeIndex] = x;
            }
            ++writeIndex;
        }
    }

    if( writeIndex != source.Size() ) source.Resize( writeIndex );
}
