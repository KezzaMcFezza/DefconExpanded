#include "systemiv.h"

#include "lib/debug/debug_utils.h"
#include "hash_table.h"


template <class T>
unsigned int HashTable<T>::HashFunc( std::string_view key ) const noexcept
{
	//
	// FNV-1a hash variant
	// Processes 4 bytes at a time when possible for better performance

	unsigned int hash = 2166136261U; // FNV offset
	size_t len = key.length();
	
	if ( len == 0 )
	{
		return 0;
	}	
	
	const char* data = key.data();
	size_t i = 0;
	
	//
	// Process 4 byte chunks

	while ( i + 4 <= len )
	{
		//
		// XOR and multiply in one go (case insensitive)

		unsigned int chunk = 0;
		chunk |= ( static_cast<unsigned char>( data[i] ) & 0xDF );
		chunk |= ( static_cast<unsigned char>( data[i + 1] ) & 0xDF ) << 8;
		chunk |= ( static_cast<unsigned char>( data[i + 2] ) & 0xDF ) << 16;
		chunk |= ( static_cast<unsigned char>( data[i + 3] ) & 0xDF ) << 24;
		
		hash ^= chunk;
		hash *= 16777619U; // FNV prime
		i += 4;
	}
	
	while ( i < len )
	{
		hash ^= ( static_cast<unsigned char>( data[i] ) & 0xDF );
		hash *= 16777619U;
		++i;
	}
	
	return hash & m_mask;
}


template <class T>
bool HashTable<T>::CaseInsensitiveEqual( std::string_view a, std::string_view b ) const noexcept
{
	return PortableCaseInsensitiveEqual( a, b );
}


template <class T>
unsigned int HashTable<T>::HashFunc( const char* key ) const noexcept
{
	if ( !key || !*key )
	{
		return 0;
	}

	unsigned int hash = 2166136261U;
	const char* p = key;
	
	while ( p[0] && p[1] && p[2] && p[3] )
	{
		unsigned int chunk = 0;
		chunk |= ( static_cast<unsigned char>( p[0] ) & 0xDF );
		chunk |= ( static_cast<unsigned char>( p[1] ) & 0xDF ) << 8;
		chunk |= ( static_cast<unsigned char>( p[2] ) & 0xDF ) << 16;
		chunk |= ( static_cast<unsigned char>( p[3] ) & 0xDF ) << 24;
		
		hash ^= chunk;
		hash *= 16777619U;
		p += 4;
	}
	
	while ( *p )
	{
		hash ^= ( static_cast<unsigned char>( *p ) & 0xDF );
		hash *= 16777619U;
		++p;
	}
	
	return hash & m_mask;
}


template <class T>
bool HashTable<T>::CaseInsensitiveEqual( const char* a, std::string_view b ) const noexcept
{
	if ( !a )
		return b.empty();
	
	return PortableCaseInsensitiveEqual( std::string_view( a ), b );
}


template <class T>
void HashTable<T>::Grow()
{
	AppReleaseAssert( m_size < 65536, "Hashtable grew too large" );

	const unsigned int oldSize = m_size;
	m_size *= 2;
	m_mask = m_size - 1;

	std::vector<std::string> oldKeys = std::move( m_keys );
	std::vector<T> oldData = std::move( m_data );

	m_keys.resize( m_size );
	m_data.resize( m_size );

	m_numCollisions = 0;

	for ( unsigned int i = 0; i < oldSize; ++i )
	{
		if ( !oldKeys[i].empty() )
		{
			const unsigned int newIndex = GetInsertPos( oldKeys[i] );
			m_keys[newIndex] = std::move( oldKeys[i] );
			m_data[newIndex] = std::move( oldData[i] );
		}
	}

	m_slotsFree += m_size - oldSize;
}


template <class T>
void HashTable<T>::Rebuild()
{
	std::vector<std::string> oldKeys = std::move( m_keys );
	std::vector<T> oldData = std::move( m_data );

	m_keys.resize( m_size );
	m_data.resize( m_size );

	m_numCollisions = 0;

	for ( unsigned int i = 0; i < m_size; ++i )
	{
		if ( !oldKeys[i].empty() )
		{
			const unsigned int newIndex = GetInsertPos( oldKeys[i] );
			m_keys[newIndex] = std::move( oldKeys[i] );
			m_data[newIndex] = std::move( oldData[i] );
		}
	}
}


template <class T>
unsigned int HashTable<T>::GetInsertPos( std::string_view key ) const
{
	unsigned int index = HashFunc( key );

	while ( !m_keys[index].empty() )
	{
		if ( CaseInsensitiveEqual( m_keys[index], key ) )
		{
			AppDebugOut( "GetInsertPos critical error : trying to insert key '%.*s'\n",
						 static_cast<int>( key.length() ), key.data() );
			DumpKeys();
			AppAbort( "Error with HashTable" );
		}

		index++;
		index &= m_mask;
		m_numCollisions++;
	}

	return index;
}


template <class T>
HashTable<T>::HashTable()
	: m_slotsFree( 4 ), m_size( 4 ), m_mask( 3 ), m_numCollisions( 0 )
{
	m_keys.resize( m_size );
	m_data.resize( m_size );
}


template <class T>
HashTable<T>::~HashTable()
{
	Empty();
}


template <class T>
void HashTable<T>::Empty()
{
	for ( unsigned int i = 0; i < m_size; ++i )
	{
		m_keys[i].clear();
		m_data[i] = T();
	}
	m_slotsFree = m_size;
}


template <class T>
int HashTable<T>::GetIndex( std::string_view key ) const
{
	unsigned int index = HashFunc( key );

	if ( m_keys[index].empty() )
	{
		return -1;
	}

	while ( !CaseInsensitiveEqual( m_keys[index], key ) )
	{
		index++;
		index &= m_mask;

		if ( m_keys[index].empty() )
		{
			return -1;
		}
	}

	return static_cast<int>( index );
}


template <class T>
int HashTable<T>::GetIndex( const char* key ) const
{
	if ( !key || !*key )
	{
		//
		// Empty/null key

		unsigned int index = HashFunc( "" );
		if ( !m_keys[index].empty() && CaseInsensitiveEqual( m_keys[index], std::string_view( "" ) ) )
		{
			return static_cast<int>( index );
		}
		return -1;
	}
	
	unsigned int index = HashFunc( key );

	if ( m_keys[index].empty() )
	{
		return -1;
	}

	//
	// Compare directly without creating string_view for the key
	
	while ( !CaseInsensitiveEqual( key, m_keys[index] ) )
	{
		index++;
		index &= m_mask;

		if ( m_keys[index].empty() )
		{
			return -1;
		}
	}

	return static_cast<int>( index );
}


template <class T>
int HashTable<T>::PutData( std::string_view key, const T &data )
{
	if ( m_slotsFree * 2 <= m_size )
	{
		Grow();
	}

	const unsigned int index = GetInsertPos( key );
	AppAssert( m_keys[index].empty() );
	m_keys[index] = key;
	m_data[index] = data;
	m_slotsFree--;

	return static_cast<int>( index );
}


template <class T>
int HashTable<T>::PutData( std::string_view key, T &&data )
{
	if ( m_slotsFree * 2 <= m_size )
	{
		Grow();
	}

	const unsigned int index = GetInsertPos( key );
	AppAssert( m_keys[index].empty() );
	m_keys[index] = key;
	m_data[index] = std::move( data );
	m_slotsFree--;

	return static_cast<int>( index );
}


template <class T>
T HashTable<T>::GetData( std::string_view key, const T &defaultValue ) const
{
	const int index = GetIndex( key );
	if ( index >= 0 )
	{
		return m_data[index];
	}
	return defaultValue;
}


template <class T>
T HashTable<T>::GetData( unsigned int index ) const
{
	return m_data[index];
}


template <class T>
std::optional<T> HashTable<T>::GetDataOptional( std::string_view key ) const
{
	const int index = GetIndex( key );
	if ( index >= 0 )
	{
		return m_data[index];
	}
	return std::nullopt;
}


template <class T>
T *HashTable<T>::GetPointer( std::string_view key ) const
{
	const int index = GetIndex( key );
	if ( index >= 0 )
	{
		return const_cast<T *>( &m_data[index] );
	}
	return nullptr;
}


template <class T>
T *HashTable<T>::GetPointer( unsigned int index ) const
{
	return const_cast<T *>( &m_data[index] );
}


template <class T>
const T *HashTable<T>::GetPointerConst( std::string_view key ) const
{
	const int index = GetIndex( key );
	if ( index >= 0 )
	{
		return &m_data[index];
	}
	return nullptr;
}


template <class T>
const T *HashTable<T>::GetPointerConst( unsigned int index ) const
{
	return &m_data[index];
}


template <class T>
void HashTable<T>::RemoveData( std::string_view key )
{
	const int index = GetIndex( key );
	if ( index >= 0 )
	{
		RemoveData( static_cast<unsigned int>( index ) );
	}
}


template <class T>
void HashTable<T>::MarkNotUsed( unsigned int index )
{
	m_keys[index].clear();
	m_slotsFree++;
}


template <class T>
void HashTable<T>::RemoveData( unsigned int index )
{
	MarkNotUsed( index );
	Rebuild();
}


template <class T>
bool HashTable<T>::ValidIndex( unsigned int index ) const noexcept
{
	return index < m_size && !m_keys[index].empty();
}


template <class T>
void HashTable<T>::Reserve( unsigned int capacity )
{
	unsigned int newSize = 4;
	while ( newSize < capacity * 2 )
	{
		newSize *= 2;
	}

	if ( newSize > m_size )
	{
		const unsigned int oldSize = m_size;
		m_size = newSize;
		m_mask = m_size - 1;

		std::vector<std::string> oldKeys = std::move( m_keys );
		std::vector<T> oldData = std::move( m_data );

		m_keys.resize( m_size );
		m_data.resize( m_size );

		m_numCollisions = 0;

		for ( unsigned int i = 0; i < oldSize; ++i )
		{
			if ( !oldKeys[i].empty() )
			{
				const unsigned int newIndex = GetInsertPos( oldKeys[i] );
				m_keys[newIndex] = std::move( oldKeys[i] );
				m_data[newIndex] = std::move( oldData[i] );
			}
		}

		m_slotsFree += m_size - oldSize;
	}
}


template <class T>
T HashTable<T>::operator[]( unsigned int index ) const
{
	return m_data[index];
}


template <class T>
const char *HashTable<T>::GetName( unsigned int index ) const
{
	return m_keys[index].c_str();
}


template <class T>
const std::string &HashTable<T>::GetNameString( unsigned int index ) const
{
	return m_keys[index];
}


template <class T>
void HashTable<T>::DumpKeys() const
{
	for ( unsigned i = 0; i < m_size; ++i )
	{
		if ( !m_keys[i].empty() )
		{
			const int hash = HashFunc( m_keys[i] );
			const int numCollisions = ( i - hash + m_size ) & m_mask;
			AppDebugOut( "%03d: %s - %d\n", i, m_keys[i].c_str(), numCollisions );
		}
		else
		{
			AppDebugOut( "%03d: empty\n", i );
		}
	}
}


// Iterator implementations
template <class T>
typename HashTable<T>::iterator HashTable<T>::begin()
{
	return iterator( this, 0 );
}


template <class T>
typename HashTable<T>::iterator HashTable<T>::end()
{
	return iterator( this, m_size );
}


template <class T>
typename HashTable<T>::const_iterator HashTable<T>::begin() const
{
	return const_iterator( this, 0 );
}


template <class T>
typename HashTable<T>::const_iterator HashTable<T>::end() const
{
	return const_iterator( this, m_size );
}


template <class T>
typename HashTable<T>::const_iterator HashTable<T>::cbegin() const
{
	return const_iterator( this, 0 );
}


template <class T>
typename HashTable<T>::const_iterator HashTable<T>::cend() const
{
	return const_iterator( this, m_size );
}