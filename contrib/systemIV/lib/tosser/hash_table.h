#ifndef INCLUDED_HASH_TABLE_H
#define INCLUDED_HASH_TABLE_H

#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <algorithm>
#include <cctype>
#include <type_traits>

//=================================================================
// Hash Table with case-insensitive string keys
//
// Uses open addressing with linear probing
// Keys are case-insensitive 
// Automatically grows when load factor exceeds 50%
//=================================================================

inline bool PortableCaseInsensitiveEqual(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    
    return std::equal(a.begin(), a.end(), b.begin(),
        [](char ca, char cb) {
            return std::tolower(static_cast<unsigned char>(ca)) == 
                   std::tolower(static_cast<unsigned char>(cb));
        });
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
    [[nodiscard]] bool CaseInsensitiveEqual( std::string_view a, std::string_view b ) const noexcept;

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
    [[nodiscard]] int GetIndex ( const char* key ) const {
        return GetIndex        ( std::string_view(key ? key : "") );
    }
    
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