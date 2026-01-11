#include "systemiv.h"

#include <vector>
#include <algorithm>
#include <utility>

#include "lib/debug/debug_utils.h"
#include "darray.h"


template <class T>
DArray<T>::DArray()
    : m_stepSize(1)
{
}


template <class T>
DArray<T>::DArray( int newstepsize )
    : m_stepSize(newstepsize)
{
}


template <class T>
DArray<T>::~DArray()
{
    Empty();
}


template <class T>
void DArray<T>::SetSize( int newsize )
{
    const int currentSize = static_cast<int>(array.size());
    
    if ( newsize > currentSize ) 
    {
        array.resize(newsize);
        shadow.resize(newsize, 0);
    }
    else if ( newsize < currentSize ) 
    {
        array.resize(newsize);
        shadow.resize(newsize);
    }
}


template <class T>
void DArray<T>::Grow()
{
    const int currentSize = static_cast<int>(array.size());
    
    if (m_stepSize == -1)
    {
        // Double array size
        if (currentSize == 0)
        {
            SetSize(1);
        }
        else
        {
            SetSize( currentSize * 2 );
        }
    }
    else
    {
        SetSize( currentSize + m_stepSize );
    }
}


template <class T>
void DArray<T>::SetStepSize( int newstepsize ) noexcept
{
    m_stepSize = newstepsize;
}


template <class T>
void DArray<T>::SetStepDouble() noexcept
{
    m_stepSize = -1;
}


template <class T>
int DArray<T>::PutData( const T &newdata )
{
    const int currentSize = static_cast<int>(array.size());
    int freeslot = -1;
    
    for ( int a = 0; a < currentSize; ++a ) 
    {
        if ( shadow[a] == 0 ) 
        {
            freeslot = a;
            break;
        }
    }
    
    if ( freeslot == -1 )
    {
        freeslot = currentSize;
        Grow();
    }
    
    array[freeslot] = newdata;
    shadow[freeslot] = 1;
    
    return freeslot;
}


template <class T>
int DArray<T>::PutData( T &&newdata )
{
    const int currentSize = static_cast<int>(array.size());
    int freeslot = -1;
    
    for ( int a = 0; a < currentSize; ++a ) 
    {
        if ( shadow[a] == 0 ) 
        {
            freeslot = a;
            break;
        }
    }
    
    if ( freeslot == -1 )
    {
        freeslot = currentSize;
        Grow();
    }
    
    array[freeslot] = std::move(newdata);
    shadow[freeslot] = 1;
    
    return freeslot;
}


template <class T>
void DArray<T>::PutData( const T &newdata, int index )
{
    AppAssert( index >= 0 );

    while( index >= static_cast<int>(array.size()) )
    {
        Grow();
    }

    array[index] = newdata;
    shadow[index] = 1;
}


template <class T>
void DArray<T>::PutData( T &&newdata, int index )
{
    AppAssert( index >= 0 );

    while( index >= static_cast<int>(array.size()) )
    {
        Grow();
    }

    array[index] = std::move(newdata);
    shadow[index] = 1;
}


template <class T>
void DArray<T>::Empty() noexcept
{
    array.clear();
    shadow.clear();
}


template <class T>
template<typename U>
std::enable_if_t<std::is_pointer_v<U>> DArray<T>::EmptyAndDelete()
{
    const int currentSize = static_cast<int>(array.size());
    for (int i = 0; i < currentSize; ++i)
    {
        if (ValidIndex(i))
        {
            delete array[i];
            array[i] = nullptr;
        }
    }

    Empty();
}


template <class T>
template<typename U>
std::enable_if_t<std::is_pointer_v<U>> DArray<T>::EmptyAndDeleteArray()
{
    const int currentSize = static_cast<int>(array.size());
    for (int i = 0; i < currentSize; ++i)
    {
        if (ValidIndex(i))
        {
            delete[] array[i];
            array[i] = nullptr;
        }
    }

    Empty();
}


template <class T>
T DArray<T>::GetData( int index ) const
{
    AppAssert(index < static_cast<int>(array.size()) && index >= 0);
    AppAssert(shadow[index] != 0);
    
    return array[index];
}


template <class T>
T *DArray<T>::GetPointer( int index ) const
{
    AppAssert( index < static_cast<int>(array.size()) && index >= 0 );
    AppAssert( shadow[index] != 0 );

    return const_cast<T*>(&array[index]);
}


template <class T>
T& DArray<T>::operator [] (int index)
{
    AppAssert( index < static_cast<int>(array.size()) && index >= 0 );
    AppAssert( shadow[index] != 0 );
    
    return array[index];
}


template <class T>
const T& DArray<T>::operator [] (int index) const
{
    AppAssert( index < static_cast<int>(array.size()) && index >= 0 );
    AppAssert( shadow[index] != 0 );
    
    return array[index];
}


template <class T>
void DArray<T>::MarkUsed( int index )
{
    AppAssert( index < static_cast<int>(array.size()) && index >= 0 );
    AppAssert( shadow[index] == 0 );
    
    shadow[index] = 1;
}


template <class T>
void DArray<T>::RemoveData( int index )
{
    AppAssert( index < static_cast<int>(array.size()) && index >= 0 );
    AppAssert( shadow[index] != 0 );
    
    shadow[index] = 0;
}


template <class T>
int DArray<T>::NumUsed() const noexcept
{
    int count = 0;
    const int currentSize = static_cast<int>(array.size());
    
    for ( int a = 0; a < currentSize; ++a )
    {
        if ( shadow[a] == 1 ) ++count;
    }
    
    return count;
}


template <class T>
constexpr int DArray<T>::Size() const noexcept
{
    return static_cast<int>(array.size());
}


template <class T>
bool DArray<T>::ValidIndex( int index ) const noexcept
{
    if (index >= static_cast<int>(array.size()) || index < 0 )
    {
        return false;
    }
    
    if (!shadow[index])
    {
        return false;
    }
    
    return true;
}


template <class T>
int DArray<T>::FindData( const T &newdata ) const
{
    const int currentSize = static_cast<int>(array.size());
    for ( int a = 0; a < currentSize; ++a )
    {
        if ( shadow[a] )
        {
            if ( array[a] == newdata ) return a;
        }
    }
    
    return -1;
}


template <class T>
void DArray<T>::Compact()
{
    const int numUsed = NumUsed();
    const int currentSize = static_cast<int>(array.size());
    
    if (numUsed == 0)
    {
        Empty();
        return;
    }
    
    if (numUsed == currentSize)
    {
        return;
    }
    
    std::vector<T> newArray;
    std::vector<char> newShadow;
    newArray.reserve(numUsed);
    newShadow.reserve(numUsed);
    
    for (int i = 0; i < currentSize; ++i)
    {
        if (shadow[i] == 1)
        {
            newArray.push_back(std::move(array[i]));
            newShadow.push_back(1);
        }
    }
    
    array = std::move(newArray);
    shadow = std::move(newShadow);
}


template <class T>
bool DArray<T>::ShouldCompact( int &counter, int checkFrequency )
{
    if( ++counter >= checkFrequency )
    {
        counter = 0;
        
        const int used = NumUsed();
        const int size = Size();
        
        if( size > 0 )
        {
            const float wastePercent = 100.0f * (size - used) / size;
            
            // Tiered compaction thresholds based on array size
            if     ( size >= 1000 ) return (wastePercent >= 15.0f);
            else if( size >= 500 )  return (wastePercent >= 20.0f);
            else if( size >= 100 )  return (wastePercent >= 25.0f);
            else if( size >= 50 )   return (wastePercent >= 35.0f);
        }
    }
    
    return false;
}


template <class T>
void DArray<T>::Reserve( int capacity )
{
    if (capacity > static_cast<int>(array.capacity()))
    {
        array.reserve(capacity);
        shadow.reserve(capacity);
    }
}


template <class T>
int DArray<T>::Capacity() const noexcept
{
    return static_cast<int>(array.capacity());
}


// Iterator implementations
template <class T>
typename DArray<T>::iterator DArray<T>::begin()
{
    int idx = 0;
    while (idx < Size() && !ValidIndex(idx))
        ++idx;
    return iterator(this, idx);
}


template <class T>
typename DArray<T>::iterator DArray<T>::end()
{
    return iterator(this, Size());
}


template <class T>
typename DArray<T>::const_iterator DArray<T>::begin() const
{
    int idx = 0;
    while (idx < Size() && !ValidIndex(idx))
        ++idx;
    return const_iterator(this, idx);
}


template <class T>
typename DArray<T>::const_iterator DArray<T>::end() const
{
    return const_iterator(this, Size());
}


template <class T>
typename DArray<T>::const_iterator DArray<T>::cbegin() const
{
    return begin();
}


template <class T>
typename DArray<T>::const_iterator DArray<T>::cend() const
{
    return end();
}