#ifndef _included_darray_h
#define _included_darray_h

#include <vector>
#include <type_traits>

//=================================================================
// Dynamic array template class
// Use : A dynamically sized list of data
// Which can be indexed into - an entry's index never changes
// A record is kept of which elements are in use. Accesses to 
// elements that are not in use will cause an assert.
//=================================================================


template <class T>
class DArray
{
protected:
    int                  m_stepSize;                                    // -1 means each step doubles the array size
    std::vector<T>       array;
    std::vector<char>    shadow;                                        // 0=not used, 1=used
    
    void        Grow();

public:
    DArray();

    explicit DArray      ( int newstepsize );

    ~DArray();
    
    DArray            ( const DArray& other ) = default;
    DArray            ( DArray&& other ) noexcept = default;
    DArray& operator= ( const DArray& other ) = default;
    DArray& operator= ( DArray&& other ) noexcept = default;

    void        SetSize         ( int newsize );
    void        SetStepSize     ( int newstepsize ) noexcept;
    void        SetStepDouble   () noexcept;                            // Switches to array size doubling when growing

    [[nodiscard]] T     GetData     ( int index ) const;
    [[nodiscard]] T*    GetPointer  ( int index ) const;

    int         PutData         ( const T &newdata );                   // Returns index used
    void        PutData         ( const T &newdata, int index );
    
    int         PutData         ( T &&newdata );                        // Returns index used
    void        PutData         ( T &&newdata, int index );
    
    void        RemoveData      ( int index );
    void        MarkUsed        ( int index );

    [[nodiscard]] int FindData  ( const T &data ) const;                // -1 means 'not found'
    [[nodiscard]] int  NumUsed  () const noexcept;                      // Returns the number of used entries
    [[nodiscard]] constexpr int Size () const noexcept;                 // Returns the total size of the array
    
    [[nodiscard]] bool ValidIndex ( int index ) const noexcept;         // Returns true if the index contains used data
    
    void Empty              () noexcept;                                // Resets the array to empty
    
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>> EmptyAndDelete();            // Delete pointed-to elements
    
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>> EmptyAndDeleteArray();       // Delete[] pointed-to elements
    
    void Compact        ();                                             // Removes all dead slots
    [[nodiscard]] bool ShouldCompact ( int &counter, int checkFrequency ); // Returns true if array should be compacted
    
    void Reserve        ( int capacity );
    [[nodiscard]] int Capacity () const noexcept;
    
    T& operator         [] (int index);
    const T& operator   [] (int index) const;
    
    //
    // Iterates only used elements

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
// Iterator class for range based for loops

template <class T>
class DArray<T>::iterator
{
    friend class DArray<T>;
    DArray<T>* parent;
    int index;
    
    iterator(DArray<T>* p, int idx) : parent(p), index(idx) {}
    
    void advance()
    {
        if (parent)
        {
            ++index;
            while (index < parent->Size() && !parent->ValidIndex(index))
                ++index;
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
    
    reference operator*() const { return (*parent)[index]; }
    pointer operator->() const { return &(*parent)[index]; }
    
    bool operator==(const iterator& other) const 
    { 
        return parent == other.parent && index == other.index; 
    }
    bool operator!=(const iterator& other) const { return !(*this == other); }
    
    [[nodiscard]] int get_index() const noexcept { return index; }
};


template <class T>
class DArray<T>::const_iterator
{
    friend class DArray<T>;
    const DArray<T>* parent;
    int index;
    
    const_iterator(const DArray<T>* p, int idx) : parent(p), index(idx) {}
    
    void advance()
    {
        if (parent)
        {
            ++index;
            while (index < parent->Size() && !parent->ValidIndex(index))
                ++index;
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
    
    reference operator*() const { return (*parent)[index]; }
    pointer operator->() const { return &(*parent)[index]; }
    
    bool operator==(const const_iterator& other) const 
    { 
        return parent == other.parent && index == other.index; 
    }
    bool operator!=(const const_iterator& other) const { return !(*this == other); }
    
    [[nodiscard]] int get_index() const noexcept { return index; }
};


//  ===================================================================

#include "darray.cpp"

#endif