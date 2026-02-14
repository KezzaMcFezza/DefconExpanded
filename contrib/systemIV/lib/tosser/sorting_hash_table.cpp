#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "sorting_hash_table.h"


//*****************************************************************************
// Protected Functions
//*****************************************************************************

template <class T>
void SortingHashTable<T>::Grow()
{
	const unsigned int oldSize = this->m_size;
	this->m_size *= 2;
	this->m_mask = ( this->m_mask << 1 ) + 1;

	std::vector<std::string> oldKeys = std::move( this->m_keys );
	std::vector<T> oldData = std::move( this->m_data );
	std::vector<short> oldOrderedIndices = std::move( m_orderedIndices );

	this->m_keys.resize( this->m_size );
	this->m_data.resize( this->m_size );
	m_orderedIndices.resize( this->m_size, -2 );

	// Go through the existing ordered index list, inserting elements into the
	// new table as we go
	short oldI = m_firstOrderedIndex;
	if ( oldI != -1 && !oldKeys[oldI].empty() )
	{
		short newI = this->GetInsertPos( oldKeys[oldI] );
		m_firstOrderedIndex = newI;

		while ( oldI != -1 )
		{
			const short nextOldI = oldOrderedIndices[oldI];

			this->m_keys[newI] = std::move( oldKeys[oldI] );
			this->m_data[newI] = std::move( oldData[oldI] );

			const short nextNewI = ( nextOldI != -1 && !oldKeys[nextOldI].empty() )
									   ? this->GetInsertPos( oldKeys[nextOldI] )
									   : -1;

			m_orderedIndices[newI] = nextNewI;

			oldI = nextOldI;
			newI = nextNewI;
		}
	}

	this->m_slotsFree += this->m_size - oldSize;
}


template <class T>
short SortingHashTable<T>::FindPrevKey( std::string_view key ) const
{
	short prevI = -1;
	short i = m_firstOrderedIndex;

	while ( true )
	{
		if ( i == -1 )
			return prevI;

		if ( PortableCaseInsensitiveCompare( this->m_keys[i], key ) >= 0 )
		{
			return prevI;
		}

		prevI = i;
		i = m_orderedIndices[i];
	}
}


//*****************************************************************************
// Public Functions
//*****************************************************************************

template <class T>
SortingHashTable<T>::SortingHashTable()
	: HashTable<T>(), m_firstOrderedIndex( -1 ), m_nextOrderedIndex( -1 )
{
	m_orderedIndices.resize( this->m_size, -2 );
}


template <class T>
SortingHashTable<T>::~SortingHashTable()
{
}


template <class T>
int SortingHashTable<T>::PutData( std::string_view key, const T &data )
{
	// Make sure the table is big enough
	if ( this->m_slotsFree * 2 <= this->m_size )
	{
		Grow();
	}

	// Do the main insert
	const unsigned int index = this->GetInsertPos( key );
	AppDebugAssert( this->m_keys[index].empty() );
	this->m_keys[index] = key;
	this->m_data[index] = data;
	this->m_slotsFree--;

	// Insert into alphabetically ordered index list
	const short i = FindPrevKey( key );
	if ( i == -1 )
	{
		// Handle the case when the table is empty, or the new element is going
		// to be the new first alphabetical element
		m_orderedIndices[index] = m_firstOrderedIndex;
		m_firstOrderedIndex = static_cast<short>( index );
	}
	else
	{
		m_orderedIndices[index] = m_orderedIndices[i];
		m_orderedIndices[i] = static_cast<short>( index );
	}

	return static_cast<int>( index );
}


template <class T>
int SortingHashTable<T>::PutData( std::string_view key, T &&data )
{
	// Make sure the table is big enough
	if ( this->m_slotsFree * 2 <= this->m_size )
	{
		Grow();
	}

	// Do the main insert with move
	const unsigned int index = this->GetInsertPos( key );
	AppDebugAssert( this->m_keys[index].empty() );
	this->m_keys[index] = key;
	this->m_data[index] = std::move( data );
	this->m_slotsFree--;

	// Insert into alphabetically ordered index list
	const short i = FindPrevKey( key );
	if ( i == -1 )
	{
		// Handle the case when the table is empty, or the new element is going
		// to be the new first alphabetical element
		m_orderedIndices[index] = m_firstOrderedIndex;
		m_firstOrderedIndex = static_cast<short>( index );
	}
	else
	{
		m_orderedIndices[index] = m_orderedIndices[i];
		m_orderedIndices[i] = static_cast<short>( index );
	}

	return static_cast<int>( index );
}


template <class T>
void SortingHashTable<T>::RemoveData( std::string_view key )
{
	const int index = this->GetIndex( key );
	if ( index >= 0 )
	{
		RemoveData( static_cast<unsigned int>( index ) );
	}
}


template <class T>
void SortingHashTable<T>::RemoveData( unsigned int _index )
{
	// Remove data
	this->m_keys[_index].clear();
	this->m_slotsFree++;

	// Remove from ordered list
	short prevIndex = -1;
	short index = m_firstOrderedIndex;

	while ( index != -1 && static_cast<unsigned int>( index ) != _index )
	{
		prevIndex = index;
		index = m_orderedIndices[index];
	}

	if ( index == -1 )
	{
		return;
	}

	if ( prevIndex == -1 )
	{
		m_firstOrderedIndex = m_orderedIndices[index];
	}
	else
	{
		m_orderedIndices[prevIndex] = m_orderedIndices[index];
	}

	m_orderedIndices[index] = -2;
}


template <class T>
short SortingHashTable<T>::StartOrderedWalk()
{
	if ( m_firstOrderedIndex == -1 )
	{
		m_nextOrderedIndex = -1;
		return -1;
	}

	m_nextOrderedIndex = m_orderedIndices[m_firstOrderedIndex];
	return m_firstOrderedIndex;
}


template <class T>
short SortingHashTable<T>::GetNextOrderedIndex()
{
	const short rv = m_nextOrderedIndex;
	if ( rv != -1 )
	{
		m_nextOrderedIndex = m_orderedIndices[m_nextOrderedIndex];
	}
	return rv;
}


// Sorted iterator implementations
template <class T>
typename SortingHashTable<T>::sorted_iterator SortingHashTable<T>::sorted_begin()
{
	return sorted_iterator( this, m_firstOrderedIndex );
}


template <class T>
typename SortingHashTable<T>::sorted_iterator SortingHashTable<T>::sorted_end()
{
	return sorted_iterator( this, -1 );
}


template <class T>
typename SortingHashTable<T>::const_sorted_iterator SortingHashTable<T>::sorted_begin() const
{
	return const_sorted_iterator( this, m_firstOrderedIndex );
}


template <class T>
typename SortingHashTable<T>::const_sorted_iterator SortingHashTable<T>::sorted_end() const
{
	return const_sorted_iterator( this, -1 );
}


template <class T>
typename SortingHashTable<T>::const_sorted_iterator SortingHashTable<T>::sorted_cbegin() const
{
	return const_sorted_iterator( this, m_firstOrderedIndex );
}


template <class T>
typename SortingHashTable<T>::const_sorted_iterator SortingHashTable<T>::sorted_cend() const
{
	return const_sorted_iterator( this, -1 );
}