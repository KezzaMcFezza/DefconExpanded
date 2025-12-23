#ifndef _included_fdarray_h
#define _included_fdarray_h

#include "darray.h"

//=================================================================
// Fast Dynamic array object
// Use : A dynamically sized list of data
// Which can be indexed into - an entry's index never changes
// Same as DArray, but has new advantages: Insert is MUCH faster
//=================================================================


template <class T>
class FastDArray: public DArray<T>
{
protected:
    int                  numused;
    std::vector<int>     freelist;
    int                  firstfree;

    void RebuildFreeList();                                     // O(n) SLOW
    void RebuildNumUsed();                                      // O(n) SLOW
    void Grow();                                                // O(n) SLOW

public:
    FastDArray();
    explicit FastDArray   (int newstepsize);
    
    FastDArray            ( const FastDArray& other ) = default;
    FastDArray            ( FastDArray&& other ) noexcept;
    FastDArray& operator= ( const FastDArray& other ) = default;
    FastDArray& operator= ( FastDArray&& other ) noexcept;
    ~FastDArray           () = default;

    void        SetSize( int newsize );                        // O(n) SLOW
    void        Reserve (int capacity );                       // O(n) but efficient
    
    [[nodiscard]] int  PutData( const T &newdata );            // O(1) FAST - Returns index used
    [[nodiscard]] int  PutData( T &&newdata );                 // O(1) FAST - Move version

    void        PutData( const T &newdata, int index );        // O(n) SLOW
    void        PutData( T &&newdata, int index );             // O(n) SLOW
    
    void        MarkUsed  ( int index );                       // O(n) SLOW
    void        RemoveData( int index );                       // O(1) FAST
    
    [[nodiscard]] T*   GetPointer();                           // O(1) FAST - Returns next free element
    [[nodiscard]] T*   GetPointer( int index );                // O(1) FAST - Returns element at index
    [[nodiscard]] int  GetNextFree();                          // O(1) FAST - Returns and marks next free index
    [[nodiscard]] constexpr int NumUsed() const noexcept;      // O(1) FAST - Cached value
       
    void        Empty() noexcept;                              // O(1) FAST - Resets the array
    
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>> EmptyAndDelete();      // O(n) but FAST iteration
    
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>> EmptyAndDeleteArray(); // O(n) but FAST iteration

    [[nodiscard]] bool ValidateFreeList() const;
};


//  ===================================================================

#include "fast_darray.cpp"

#endif