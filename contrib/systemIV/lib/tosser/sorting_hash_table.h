#ifndef INCLUDED_SORTING_HASH_TABLE_H
#define INCLUDED_SORTING_HASH_TABLE_H

#include <string_view>
#include "hash_table.h"

//=================================================================
// Sorting Hash Table - extends HashTable
//
// Maintains elements in alphabetically sorted order by key
// Provides efficient iteration in sorted order
//
// Trade-offs:
// - PutData: O(n) instead of O(1) due to sorted insert
// - RemoveData: O(n) instead of O(1) due to sorted list update
// - Iteration: Same O(n) but yields elements in sorted order
// - No need to skip empty slots during iteration
//=================================================================

inline int PortableCaseInsensitiveCompare(std::string_view a, std::string_view b) noexcept
{
    auto it1 = a.begin();
    auto it2 = b.begin();
    
    while (it1 != a.end() && it2 != b.end()) {
        const auto c1 = std::tolower(static_cast<unsigned char>(*it1));
        const auto c2 = std::tolower(static_cast<unsigned char>(*it2));
        
        if (c1 < c2) return -1;
        if (c1 > c2) return 1;
        
        ++it1;
        ++it2;
    }
    
    if (a.size() < b.size()) return -1;
    if (a.size() > b.size()) return 1;
    return 0;
}


template <class T>
class SortingHashTable: public HashTable<T>
{
protected:
    std::vector<short> m_orderedIndices;        // Chain of indices in alphabetical order
    short m_firstOrderedIndex;                  // Index of alphabetically first element
    mutable short m_nextOrderedIndex;           // Used by GetNextOrderedIndex (mutable for const iteration)

    void Grow();
    [[nodiscard]] short FindPrevKey(std::string_view key) const;  // Returns index of element alphabetically before key

public:
    SortingHashTable();
    SortingHashTable( const SortingHashTable& ) = default;
    SortingHashTable( SortingHashTable&& ) noexcept = default;
    ~SortingHashTable();
    
    SortingHashTable& operator=( const SortingHashTable& ) = default;
    SortingHashTable& operator=( SortingHashTable&& ) noexcept = default;

    int PutData( std::string_view key, const T& data );      // O(n) - maintains sorted order
    int PutData( std::string_view key, T&& data );           // O(n) - move version
    
    int PutData( const char* key, const T& data ) {
        return PutData( std::string_view(key ? key : "" ), data);
    }
    int PutData( const char* key, T&& data ) {
        return PutData( std::string_view(key ? key : "" ), std::move(data));
    }
    
    void RemoveData( std::string_view key );                 // O(n) - updates sorted list
    void RemoveData( unsigned int index );                   // O(n) - updates sorted list
    
    void RemoveData( const char* key ) {
        RemoveData( std::string_view(key ? key : "") );
    }

	//
    // Manual iteration in sorted order

    [[nodiscard]] short StartOrderedWalk();                // Returns first index (-1 if empty)
    [[nodiscard]] short GetNextOrderedIndex();             // Returns next index (-1 if no more)
    
	//
    // Yields elements in sorted order

    class sorted_iterator;
    class const_sorted_iterator;
    
    [[nodiscard]] sorted_iterator sorted_begin();
    [[nodiscard]] sorted_iterator sorted_end();
    [[nodiscard]] const_sorted_iterator sorted_begin() const;
    [[nodiscard]] const_sorted_iterator sorted_end() const;
    [[nodiscard]] const_sorted_iterator sorted_cbegin() const;
    [[nodiscard]] const_sorted_iterator sorted_cend() const;
    
	//
    // Note: Inherits unsorted iteration from HashTable (skips empty slots)
};


//=================================================================
// Sorted iterator, follows the ordered chain


template <class T>
class SortingHashTable<T>::sorted_iterator
{
    friend class SortingHashTable<T>;
    SortingHashTable<T>* parent;
    short index;
    
    sorted_iterator(SortingHashTable<T>* p, short idx) : parent(p), index(idx) {}
    
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;
    
    sorted_iterator& operator++() {
        if (parent && index != -1) {
            index = parent->m_orderedIndices[index];
        }
        return *this;
    }
    
    sorted_iterator operator++(int) {
        sorted_iterator tmp = *this;
        ++(*this);
        return tmp;
    }
    
    reference operator*() const { return parent->m_data[index]; }
    pointer operator->() const { return &parent->m_data[index]; }
    
    [[nodiscard]] bool operator==( const sorted_iterator& other ) const noexcept {
        return parent == other.parent && index == other.index;
    }
    [[nodiscard]] bool operator!=( const sorted_iterator& other ) const noexcept {
        return !(*this == other);
    }
    
    [[nodiscard]] short get_index() const noexcept { return index; }
    [[nodiscard]] const std::string& get_key() const { return parent->m_keys[index]; }
};


template <class T>
class SortingHashTable<T>::const_sorted_iterator
{
    friend class SortingHashTable<T>;
    const SortingHashTable<T>* parent;
    short index;
    
    const_sorted_iterator(const SortingHashTable<T>* p, short idx) : parent(p), index(idx) {}
    
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;
    
    const_sorted_iterator& operator++() {
        if (parent && index != -1) {
            index = parent->m_orderedIndices[index];
        }
        return *this;
    }
    
    const_sorted_iterator operator++(int) {
        const_sorted_iterator tmp = *this;
        ++(*this);
        return tmp;
    }
    
    reference operator*() const { return parent->m_data[index]; }
    pointer operator->() const { return &parent->m_data[index]; }
    
    [[nodiscard]] bool operator==(const const_sorted_iterator& other) const noexcept {
        return parent == other.parent && index == other.index;
    }
    [[nodiscard]] bool operator!=(const const_sorted_iterator& other) const noexcept {
        return !(*this == other);
    }
    
    [[nodiscard]] short get_index() const noexcept { return index; }
    [[nodiscard]] const std::string& get_key() const { return parent->m_keys[index]; }
};


#include "sorting_hash_table.cpp"

#endif