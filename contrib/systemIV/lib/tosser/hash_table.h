#ifndef INCLUDED_HASH_TABLE_H
#define INCLUDED_HASH_TABLE_H

#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <type_traits>

//=================================================================
// Hash Table with case-insensitive string keys
//
// Uses open addressing with linear probing
// Keys are case-insensitive 
// Automatically grows when load factor exceeds 50%
//=================================================================

inline bool PortableCaseInsensitiveEqual(std::string_view a, std::string_view b) noexcept 
{
    size_t len = a.length();
    
    if ( len != b.length() )
    {
        return false;
    }
    
    if ( len == 0 )
    {
        return true;
    }
    
    const char* data1 = a.data();
    const char* data2 = b.data();
    
    //
    // Fast path:
    // Try exact byte comparison first
    // This avoids expensive case conversion for the common case

    bool needsCaseInsensitive = false;
    if (len <= 16)
    {
        //
        // For short strings, compare directly

        for (size_t i = 0; i < len; ++i)
        {
            if (data1[i] != data2[i])
            {
                needsCaseInsensitive = true;
                break;
            }
        }
        if (!needsCaseInsensitive)
        {
            return true;
        }
    }
    else
    {
        //
        // For longer strings, use memcmp for fast comparison

        if (std::memcmp(data1, data2, len) != 0)
        {
            needsCaseInsensitive = true;
        }
        else
        {
            return true;
        }
    }
    
    //
    // Fall back to case insensitive comparison only if exact match failed

    size_t i = 0;
    
    //
    // Compare 4 bytes at a time 
    
    while (i + 4 <= len)
    {
        unsigned int chunk1 = 0;
        unsigned int chunk2 = 0;
        
        chunk1 |= (static_cast<unsigned char>(data1[i]) & 0xDF);
        chunk1 |= (static_cast<unsigned char>(data1[i + 1]) & 0xDF) << 8;
        chunk1 |= (static_cast<unsigned char>(data1[i + 2]) & 0xDF) << 16;
        chunk1 |= (static_cast<unsigned char>(data1[i + 3]) & 0xDF) << 24;
        
        chunk2 |= (static_cast<unsigned char>(data2[i]) & 0xDF);
        chunk2 |= (static_cast<unsigned char>(data2[i + 1]) & 0xDF) << 8;
        chunk2 |= (static_cast<unsigned char>(data2[i + 2]) & 0xDF) << 16;
        chunk2 |= (static_cast<unsigned char>(data2[i + 3]) & 0xDF) << 24;
        
        if (chunk1 != chunk2)
        {
            return false;
        }
        
        i += 4;
    }
    
    //
    // Handle remaining bytes
    
    while (i < len)
    {
        unsigned char c1 = static_cast<unsigned char>(data1[i]) & 0xDF;
        unsigned char c2 = static_cast<unsigned char>(data2[i]) & 0xDF;
        if (c1 != c2)
        {
            return false;
        }
        ++i;
    }
    
    return true;
}

template <class T>
class HashTable
{
protected:
    std::vector<std::string> m_keys;
    std::vector<T> m_data;
    unsigned int m_slotsFree;
    unsigned int m_size;
    unsigned int m_mask;
    mutable unsigned int m_numCollisions;
    
    [[nodiscard]] unsigned int HashFunc    ( std::string_view key ) const noexcept;
    [[nodiscard]] unsigned int HashFunc    ( const char* key )      const noexcept;
    [[nodiscard]] bool CaseInsensitiveEqual( std::string_view a, std::string_view b ) const noexcept;
    [[nodiscard]] bool CaseInsensitiveEqual( const char* a, std::string_view b )      const noexcept;

    void Grow();
    void Rebuild();

    [[nodiscard]] unsigned int GetInsertPos( std::string_view key ) const;

public:
    HashTable();
    HashTable            ( const HashTable& ) = default;
    HashTable            ( HashTable&& ) noexcept = default;
    ~HashTable();
    
    HashTable& operator= ( const HashTable& ) = default;
    HashTable& operator= ( HashTable&& ) noexcept = default;
    
    [[nodiscard]] int GetIndex ( std::string_view key ) const;
    [[nodiscard]] int GetIndex ( const char* key ) const;
    
    int PutData( std::string_view key, const T& data );
    int PutData( std::string_view key, T&& data );  // Move version
    
    int PutData( const char* key, const T& data ) {
        return PutData( std::string_view(key ? key : ""), data);
    }
    int PutData( const char* key, T&& data ) {
        return PutData( std::string_view(key ? key : ""), std::move(data) );
    }
    
	//
    // Returns pointer, but returns nullptr if not found

    [[nodiscard]] T* GetPointer           ( std::string_view key ) const;
    [[nodiscard]] T* GetPointer           ( unsigned int index ) const;
    [[nodiscard]] const T* GetPointerConst( std::string_view key ) const;
    [[nodiscard]] const T* GetPointerConst( unsigned int index ) const;
    
    [[nodiscard]] T* GetPointer( const char* key ) const {
        return GetPointer( std::string_view(key ? key : "") );
    }
    [[nodiscard]] const T* GetPointerConst( const char* key ) const {
        return GetPointerConst( std::string_view(key ? key : "") );
    }
    
	//
    // Returns value with default

    [[nodiscard]] T GetData( std::string_view key, const T& defaultValue = T() ) const;
    [[nodiscard]] T GetData( unsigned int index ) const;
    [[nodiscard]] T GetData( const char* key, const T& defaultValue = T() ) const {
        return GetData     ( std::string_view(key ? key : ""), defaultValue );
    }
    
    [[nodiscard]] std::optional<T> GetDataOptional( std::string_view key ) const;
    [[nodiscard]] std::optional<T> GetDataOptional( const char* key ) const {
        return GetDataOptional( std::string_view(key ? key : "") );
    }
    
    void RemoveData( std::string_view key );
    void RemoveData( unsigned int index );
    
    void RemoveData ( const char* key ) {
        RemoveData  ( std::string_view(key ? key : "") );
    }
    
    void MarkNotUsed( unsigned int index );
    
    void Empty();
    
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>> EmptyAndDelete() {
        for (unsigned int i = 0; i < m_size; ++i) {
            if (!m_keys[i].empty()) {
                delete m_data[i];
                m_data[i] = nullptr;
            }
        }
        Empty();
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>> EmptyAndDeleteArray() {
        for (unsigned int i = 0; i < m_size; ++i) {
            if (!m_keys[i].empty()) {
                delete[] m_data[i];
                m_data[i] = nullptr;
            }
        }
        Empty();
    }
    
    [[nodiscard]] bool ValidIndex( unsigned int index ) const noexcept;
    [[nodiscard]] constexpr unsigned int Size() const noexcept { return m_size; }
    [[nodiscard]] constexpr unsigned int NumUsed() const noexcept { return m_size - m_slotsFree; }
    [[nodiscard]] constexpr float LoadFactor() const noexcept { 
        return m_size > 0 ? static_cast<float>(NumUsed()) / m_size : 0.0f; 
    }
    
    void Reserve(unsigned int capacity);
    
    [[nodiscard]] T operator[]( unsigned int index ) const;
    [[nodiscard]] const char* GetName( unsigned int index ) const;
    [[nodiscard]] const std::string& GetNameString( unsigned int index ) const;
    
    void DumpKeys() const;
    
    class iterator;
    class const_iterator;
    
    [[nodiscard]] iterator begin();
    [[nodiscard]] iterator end();
    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] const_iterator end() const;
    [[nodiscard]] const_iterator cbegin() const;
    [[nodiscard]] const_iterator cend() const;
};


//=================================================================
// Iterator that skips empty slots


template <class T>
class HashTable<T>::iterator
{
    friend class HashTable<T>;
    HashTable<T>* parent;
    unsigned int index;
    
    iterator(HashTable<T>* p, unsigned int idx) : parent(p), index(idx) {
        while (index < parent->m_size && parent->m_keys[index].empty()) {
            ++index;
        }
    }
    
    void advance() {
        if (parent && index < parent->m_size) {
            ++index;
            while (index < parent->m_size && parent->m_keys[index].empty()) {
                ++index;
            }
        }
    }
    
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;
    
    iterator& operator++() { advance(); return *this; }
    iterator operator++(int) { iterator tmp = *this; advance(); return tmp; }
    
    reference operator*() const { return parent->m_data[index]; }
    pointer operator->() const { return &parent->m_data[index]; }
    
    [[nodiscard]] bool operator==(const iterator& other) const noexcept { 
        return parent == other.parent && index == other.index; 
    }
    [[nodiscard]] bool operator!=(const iterator& other) const noexcept { 
        return !(*this == other); 
    }
    
    [[nodiscard]] unsigned int get_index() const noexcept { return index; }
    [[nodiscard]] const std::string& get_key() const { return parent->m_keys[index]; }
};


template <class T>
class HashTable<T>::const_iterator
{
    friend class HashTable<T>;
    const HashTable<T>* parent;
    unsigned int index;
    
    const_iterator(const HashTable<T>* p, unsigned int idx) : parent(p), index(idx) {
        while (index < parent->m_size && parent->m_keys[index].empty()) {
            ++index;
        }
    }
    
    void advance() {
        if (parent && index < parent->m_size) {
            ++index;
            while (index < parent->m_size && parent->m_keys[index].empty()) {
                ++index;
            }
        }
    }
    
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = const T*;
    using reference = const T&;
    
    const_iterator& operator++() { advance(); return *this; }
    const_iterator operator++(int) { const_iterator tmp = *this; advance(); return tmp; }
    
    reference operator*() const { return parent->m_data[index]; }
    pointer operator->() const { return &parent->m_data[index]; }
    
    [[nodiscard]] bool operator==(const const_iterator& other) const noexcept { 
        return parent == other.parent && index == other.index; 
    }
    [[nodiscard]] bool operator!=(const const_iterator& other) const noexcept { 
        return !(*this == other); 
    }
    
    [[nodiscard]] unsigned int get_index() const noexcept { return index; }
    [[nodiscard]] const std::string& get_key() const { return parent->m_keys[index]; }
};


#include "hash_table.cpp"

#endif