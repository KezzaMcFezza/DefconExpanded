#include "systemiv.h"

#include <string>
#include <string_view>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <optional>
#include <unordered_map>

#include "lib/debug/debug_utils.h"
#include "lib/string_utils.h"
#include "btree.h"


template <class T>
int BTree<T>::CompareNoCase( std::string_view a, std::string_view b ) noexcept
{
	auto it1 = a.begin();
	auto it2 = b.begin();

	while ( it1 != a.end() && it2 != b.end() )
	{
		const auto c1 = std::tolower( static_cast<unsigned char>( *it1 ) );
		const auto c2 = std::tolower( static_cast<unsigned char>( *it2 ) );

		if ( c1 < c2 )
			return -1;
		if ( c1 > c2 )
			return 1;

		++it1;
		++it2;
	}

	if ( a.size() < b.size() )
		return -1;
	if ( a.size() > b.size() )
		return 1;
	return 0;
}

//
// Helper to convert string_view to lowercase string for fast lookups
// Uses a cache with transparent hash/equality to avoid string allocations on lookup
//
// C++23 transparent hash/equality allows string_view lookups without conversion

// Case-insensitive hash function that works with both string and string_view
// Optimized for speed - uses FNV-1a style hashing with bit operations
struct CaseInsensitiveHash {
	using is_transparent = void; // Enable transparent lookup (C++17/20/23)
	
	template<typename T>
	size_t operator()( const T& key ) const noexcept
	{
		// FNV-1a hash variant - faster than multiplicative hashing
		size_t hash = 2166136261U; // FNV offset basis
		size_t len = key.length();
		
		// Process 4 bytes at a time when possible (faster)
		const char* data = key.data();
		size_t i = 0;
		
		// Process 4-byte chunks
		while ( i + 4 <= len )
		{
			// XOR and multiply in one go - case-insensitive
			unsigned int chunk = 0;
			chunk |= ( static_cast<unsigned char>( data[i] ) & 0xDF );
			chunk |= ( static_cast<unsigned char>( data[i + 1] ) & 0xDF ) << 8;
			chunk |= ( static_cast<unsigned char>( data[i + 2] ) & 0xDF ) << 16;
			chunk |= ( static_cast<unsigned char>( data[i + 3] ) & 0xDF ) << 24;
			
			hash ^= chunk;
			hash *= 16777619U; // FNV prime
			i += 4;
		}
		
		// Handle remaining bytes
		while ( i < len )
		{
			hash ^= ( static_cast<unsigned char>( data[i] ) & 0xDF );
			hash *= 16777619U;
			++i;
		}
		
		return hash;
	}
};

//
// Case insensitive equality function that works with both string and string_view
// compares 4 bytes at a time when possible

struct CaseInsensitiveEqual {
	using is_transparent = void;
	
	template<typename T1, typename T2>
	bool operator()( const T1& a, const T2& b ) const noexcept
	{
		size_t len = a.length();
		if ( len != b.length() )
			return false;
		
		if ( len == 0 )
			return true;
		
		const char* data1 = a.data();
		const char* data2 = b.data();
		size_t i = 0;
		
		//
		// Compare 4 bytes at a time

		while ( i + 4 <= len )
		{
			unsigned int chunk1 = 0;
			unsigned int chunk2 = 0;
			
			chunk1 |= ( static_cast<unsigned char>( data1[i] ) & 0xDF );
			chunk1 |= ( static_cast<unsigned char>( data1[i + 1] ) & 0xDF ) << 8;
			chunk1 |= ( static_cast<unsigned char>( data1[i + 2] ) & 0xDF ) << 16;
			chunk1 |= ( static_cast<unsigned char>( data1[i + 3] ) & 0xDF ) << 24;
			
			chunk2 |= ( static_cast<unsigned char>( data2[i] ) & 0xDF );
			chunk2 |= ( static_cast<unsigned char>( data2[i + 1] ) & 0xDF ) << 8;
			chunk2 |= ( static_cast<unsigned char>( data2[i + 2] ) & 0xDF ) << 16;
			chunk2 |= ( static_cast<unsigned char>( data2[i + 3] ) & 0xDF ) << 24;
			
			if ( chunk1 != chunk2 )
				return false;
			
			i += 4;
		}

		//
		// Handle remaining bytes

		while ( i < len )
		{
			unsigned char c1 = static_cast<unsigned char>( data1[i] ) & 0xDF;
			unsigned char c2 = static_cast<unsigned char>( data2[i] ) & 0xDF;
			if ( c1 != c2 )
				return false;
			++i;
		}
		
		return true;
	}
};

static std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual> s_normalizationCache;
static constexpr size_t MAX_CACHE_SIZE = 512; // Limit cache size to prevent unbounded growth

static const std::string& ToLowerStringCached( std::string_view sv )
{
	
	//
	// Direct lookup with string_view, no conversion needed

	auto it = s_normalizationCache.find( sv );
	if ( it != s_normalizationCache.end() )
	{
		return it->second;
	}
	
	bool needsConversion = false;
	for ( unsigned char c : sv )
	{
		if ( c >= 'A' && c <= 'Z' )
		{
			needsConversion = true;
			break;
		}
	}
	
	std::string result;
	if ( !needsConversion )
	{
		result = std::string( sv );
	}
	else
	{
		//
		// Convert to lowercase

		result.reserve( sv.size() );
		for ( unsigned char c : sv )
		{
			//
			// Fast lowercase: 'A'-'Z' (65-90) becomes 'a'-'z' (97-122) by adding 32

			if ( c >= 'A' && c <= 'Z' )
			{
				result += static_cast<char>( c + 32 );
			}
			else
			{
				result += static_cast<char>( c );
			}
		}
	}
	
	if ( s_normalizationCache.size() >= MAX_CACHE_SIZE )
	{
		s_normalizationCache.clear();
	}
	
	//
    // Store in cache
	
	std::string cacheKey( sv );
	auto [inserted_it, inserted] = s_normalizationCache.emplace( std::move( cacheKey ), std::move( result ) );
	return inserted_it->second;
}

static std::string ToLowerString( std::string_view sv )
{
	return ToLowerStringCached( sv );
}


template <class T>
BTree<T>::BTree()
	: data( T() )
{
}


template <class T>
BTree<T>::BTree( std::string_view newid, const T &newdata )
	: id( ToLowerString( newid ) ), data( newdata )
{
}


template <class T>
BTree<T>::BTree( const BTree<T> &copy )
{
	Copy( copy );
}


template <class T>
BTree<T>::BTree( BTree<T> &&other ) noexcept
	: ltree( std::move( other.ltree ) ), rtree( std::move( other.rtree ) ), id( std::move( other.id ) ), data( std::move( other.data ) )
{
}


template <class T>
BTree<T> &BTree<T>::operator=( const BTree<T> &copy )
{
	if ( this != &copy )
	{
		Copy( copy );
	}
	return *this;
}


template <class T>
BTree<T> &BTree<T>::operator=( BTree<T> &&other ) noexcept
{
	if ( this != &other )
	{
		ltree = std::move( other.ltree );
		rtree = std::move( other.rtree );
		id = std::move( other.id );
		data = std::move( other.data );
	}
	return *this;
}


template <class T>
void BTree<T>::Copy( const BTree<T> &copy )
{
	if ( copy.ltree )
	{
		ltree = std::make_unique<BTree>( *copy.ltree );
	}
	else
	{
		ltree.reset();
	}

	if ( copy.rtree )
	{
		rtree = std::make_unique<BTree>( *copy.rtree );
	}
	else
	{
		rtree.reset();
	}

	id = copy.id;
	data = copy.data;
}


template <class T>
BTree<T>::~BTree()
{
	Empty();
}


template <class T>
void BTree<T>::Empty() noexcept
{
	ltree.reset();
	rtree.reset();
	id.clear();
}


template <class T>
void BTree<T>::PutData( std::string_view newid, const T &newdata )
{
	std::string normalizedId = ToLowerString( newid );
	
	if ( id.empty() )
	{
		id = normalizedId;
		data = newdata;
		return;
	}

	const int compareResult = normalizedId.compare( id );

	if ( compareResult <= 0 )
	{
		if ( ltree )
			ltree->PutData( normalizedId, newdata );
		else
			ltree = std::make_unique<BTree>( normalizedId, newdata );
	}
	else
	{
		if ( rtree )
			rtree->PutData( normalizedId, newdata );
		else
			rtree = std::make_unique<BTree>( normalizedId, newdata );
	}
}


template <class T>
void BTree<T>::RemoveDataInternal( const std::string &normalizedId )
{
	/*
	  Deletes an element from the list
	  By replacing the element with its own left node, then appending
	  its own right node onto the extreme right of itself.
	*/

	if ( id.empty() )
	{
		return;
	}

	const int compareResult = normalizedId.compare( id );

	if ( compareResult == 0 )
	{
		std::unique_ptr<BTree> tempright = std::move( rtree );

		if ( ltree )
		{
			id = std::move( ltree->id );
			data = std::move( ltree->data );

			std::unique_ptr<BTree> tempLeft = std::move( ltree->ltree );
			rtree = std::move( ltree->rtree );
			ltree = std::move( tempLeft );

			if ( tempright )
			{
				AppendRight( std::move( tempright ) );
			}
		}
		else
		{
			if ( tempright )
			{
				id = std::move( tempright->id );
				data = std::move( tempright->data );
				ltree = std::move( tempright->ltree );
				rtree = std::move( tempright->rtree );
			}
			else
			{
				id.clear();
			}
		}
	}
	else if ( compareResult < 0 )
	{
		if ( Left() )
		{
			if ( Left()->id == normalizedId && !Left()->Left() && !Left()->Right() )
			{
				ltree.reset();
			}
			else
				Left()->RemoveDataInternal( normalizedId );
		}
	}
	else
	{
		if ( Right() )
		{
			if ( Right()->id == normalizedId && !Right()->Left() && !Right()->Right() )
			{
				rtree.reset();
			}
			else
				Right()->RemoveDataInternal( normalizedId );
		}
	}
}

template <class T>
void BTree<T>::RemoveData( std::string_view newid )
{
	std::string normalizedId = ToLowerString( newid );
	RemoveDataInternal( normalizedId );
}


template <class T>
std::optional<T> BTree<T>::GetDataOpt( std::string_view searchid ) const
{
	const BTree<T> *btree = LookupTree( searchid );

	if ( btree )
		return btree->data;
	else
		return std::nullopt;
}


template <class T>
T BTree<T>::GetData( std::string_view searchid ) const
{
	const BTree<T> *btree = LookupTree( searchid );

	if ( btree )
		return btree->data;
	else
		return T(); // Return default-constructed T for backward compatibility
}


template <class T>
void BTree<T>::AppendRight( std::unique_ptr<BTree> tempright )
{
	if ( !rtree )
	{
		rtree = std::move( tempright );
	}
	else
	{
		rtree->AppendRight( std::move( tempright ) );
	}
}


template <class T>
BTree<T> *BTree<T>::LookupTreeInternal( const std::string &normalizedSearchId )
{
	if ( id.empty() )
		return nullptr;

	const int compareResult = normalizedSearchId.compare( id );

	if ( compareResult == 0 )
		return this;
	else if ( ltree && compareResult < 0 )
		return ltree->LookupTreeInternal( normalizedSearchId );
	else if ( rtree && compareResult > 0 )
		return rtree->LookupTreeInternal( normalizedSearchId );
	else
		return nullptr;
}

template <class T>
BTree<T> *BTree<T>::LookupTree( std::string_view searchid )
{
	const std::string& normalizedSearchId = ToLowerStringCached( searchid );
	return LookupTreeInternal( normalizedSearchId );
}


template <class T>
const BTree<T> *BTree<T>::LookupTreeInternal( const std::string &normalizedSearchId ) const
{
	if ( id.empty() )
		return nullptr;

	const int compareResult = normalizedSearchId.compare( id );

	if ( compareResult == 0 )
		return this;
	else if ( ltree && compareResult < 0 )
		return ltree->LookupTreeInternal( normalizedSearchId );
	else if ( rtree && compareResult > 0 )
		return rtree->LookupTreeInternal( normalizedSearchId );
	else
		return nullptr;
}

template <class T>
const BTree<T> *BTree<T>::LookupTree( std::string_view searchid ) const
{
	const std::string& normalizedSearchId = ToLowerStringCached( searchid );
	return LookupTreeInternal( normalizedSearchId );
}


template <class T>
int BTree<T>::Size() const noexcept
{
	unsigned int subsize = id.empty() ? 0 : 1;

	if ( ltree )
		subsize += ltree->Size();
	if ( rtree )
		subsize += rtree->Size();

	return subsize;
}


template <class T>
void BTree<T>::Print() const
{
	if ( ltree )
		ltree->Print();
	if ( !id.empty() )
		std::cout << id << " : " << data << "\n";
	if ( rtree )
		rtree->Print();
}


template <class T>
std::unique_ptr<DArray<T>> BTree<T>::ConvertToDArraySafe()
{
	auto darray = std::make_unique<DArray<T>>();
	RecursiveConvertToDArray( darray.get(), this );
	return darray;
}


template <class T>
std::unique_ptr<DArray<std::string>> BTree<T>::ConvertIndexToDArraySafe()
{
	auto darray = std::make_unique<DArray<std::string>>();
	RecursiveConvertIndexToDArray( darray.get(), this );
	return darray;
}


template <class T>
DArray<T> *BTree<T>::ConvertToDArray()
{
	DArray<T> *darray = new DArray<T>;
	RecursiveConvertToDArray( darray, this );
	return darray;
}


template <class T>
DArray<std::string> *BTree<T>::ConvertIndexToDArray()
{
	DArray<std::string> *darray = new DArray<std::string>;
	RecursiveConvertIndexToDArray( darray, this );
	return darray;
}


template <class T>
void BTree<T>::RecursiveConvertToDArray( DArray<T> *darray, BTree<T> *btree )
{
	AppAssert( darray );

	if ( !btree )
		return;

	if ( !btree->id.empty() )
		darray->PutData( btree->data );

	RecursiveConvertToDArray( darray, btree->Left() );
	RecursiveConvertToDArray( darray, btree->Right() );
}


template <class T>
void BTree<T>::RecursiveConvertIndexToDArray( DArray<std::string> *darray, BTree<T> *btree )
{
	AppAssert( darray );

	if ( !btree )
		return;

	if ( !btree->id.empty() )
	{
		darray->PutData( btree->id );
	}

	RecursiveConvertIndexToDArray( darray, btree->Left() );
	RecursiveConvertIndexToDArray( darray, btree->Right() );
}