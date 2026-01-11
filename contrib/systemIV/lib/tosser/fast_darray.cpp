#include "systemiv.h"

#include <vector>
#include <utility>

#include "lib/debug/debug_utils.h"
#include "fast_darray.h"


template <class T>
FastDArray<T>::FastDArray() 
    : DArray<T>()
    , numused(0)
    , firstfree(-1)
{
}


template <class T>
FastDArray<T>::FastDArray(int newstepsize) 
    : DArray<T>(newstepsize)
    , numused(0)
    , firstfree(-1)
{
}


template <class T>
FastDArray<T>::FastDArray(FastDArray&& other) noexcept
    : DArray<T>(std::move(other))
    , numused(other.numused)
    , freelist(std::move(other.freelist))
    , firstfree(other.firstfree)
{
    other.numused = 0;
    other.firstfree = -1;
}


template <class T>
FastDArray<T>& FastDArray<T>::operator=(FastDArray&& other) noexcept
{
    if (this != &other)
    {
        DArray<T>::operator=(std::move(other));
        numused = other.numused;
        freelist = std::move(other.freelist);
        firstfree = other.firstfree;
        
        other.numused = 0;
        other.firstfree = -1;
    }
    return *this;
}


template <class T>
void FastDArray<T>::RebuildFreeList()
{
    const int currentSize = static_cast<int>(this->array.size());
    freelist.resize(currentSize);
    firstfree = -1;
    int lastknownfree = -1;

    for (int i = 0; i < currentSize; ++i)
    {
        if (this->shadow[i] == 0)
        {
            if (firstfree == -1)
            {
                firstfree = i;
            }
            else
            {
                if (lastknownfree != -1)
                {
                    freelist[lastknownfree] = i;
                }
            }

            freelist[i] = -1;
            lastknownfree = i;
        }
        else
        {
            freelist[i] = -2;  // -2 indicates "in use"
        }
    }
}


template <class T>
void FastDArray<T>::RebuildNumUsed()
{
    numused = 0;
    const int currentSize = static_cast<int>(this->array.size());
    for (int i = 0; i < currentSize; ++i)
    {
        if (this->shadow[i] == 1)
            ++numused;
    }
}


template <class T>
void FastDArray<T>::SetSize(int newsize)
{
    const int currentSize = static_cast<int>(this->array.size());
    
    if (newsize > currentSize)
    {
        const int oldarraysize = currentSize;
        const int oldfirstfree = firstfree;
        
		//
        // Save old free list

        std::vector<int> oldfreelist = std::move(freelist);

		//
        // Resize arrays

        this->array.resize(newsize);
        this->shadow.resize(newsize, 0);
        freelist.resize(newsize);
        
		//
        // Restore old free list entries

        for (int a = 0; a < oldarraysize; ++a)
        {
            freelist[a] = oldfreelist[a];
        }
        
		//
        // Link new elements into free list

        firstfree = oldarraysize;
        for (int a = oldarraysize; a < newsize - 1; ++a)
        {
            freelist[a] = a + 1;
        }
        freelist[newsize - 1] = oldfirstfree;
    }
    else if (newsize < currentSize)
    {
        this->array.resize(newsize);
        this->shadow.resize(newsize);
        freelist.resize(newsize);

        RebuildNumUsed();
        RebuildFreeList();
    }
}


template <class T>
void FastDArray<T>::Grow()
{
    const int currentSize = static_cast<int>(this->array.size());
    
    if (this->m_stepSize == -1)
    {
        if (currentSize == 0)
        {
            SetSize(1);
        }
        else
        {
            SetSize(currentSize * 2);
        }
    }
    else
    {
        SetSize(currentSize + this->m_stepSize);
    }
}


template <class T>
void FastDArray<T>::Reserve(int capacity)
{
    const int currentSize = static_cast<int>(this->array.size());
    
    if (capacity <= currentSize)
        return;
    
    this->array.reserve(capacity);
    this->shadow.reserve(capacity);
    freelist.reserve(capacity);
    
    SetSize(capacity);
}


template <class T>
int FastDArray<T>::PutData(const T &newdata)
{
    if (firstfree == -1)
    {
        Grow();
    }

    AppAssert(firstfree != -1);

    const int freeslot = firstfree;
    const int nextfree = freelist[freeslot];

    this->array[freeslot] = newdata;
    if (this->shadow[freeslot] == 0)
        ++numused;
    this->shadow[freeslot] = 1;
    freelist[freeslot] = -2;
    firstfree = nextfree;
  
    return freeslot;
}


template <class T>
int FastDArray<T>::PutData(T &&newdata)
{
    if (firstfree == -1)
    {
        Grow();
    }

    AppAssert(firstfree != -1);

    const int freeslot = firstfree;
    const int nextfree = freelist[freeslot];

    this->array[freeslot] = std::move(newdata);
    if (this->shadow[freeslot] == 0)
        ++numused;
    this->shadow[freeslot] = 1;
    freelist[freeslot] = -2;
    firstfree = nextfree;
  
    return freeslot;
}


template <class T>
void FastDArray<T>::PutData(const T &newdata, int index)
{
    AppAssert(index < static_cast<int>(this->array.size()) && index >= 0);

    this->array[index] = newdata;

    if (this->shadow[index] == 0)
    {
        this->shadow[index] = 1;
        ++numused;
        RebuildFreeList();
    }
}


template <class T>
void FastDArray<T>::PutData(T &&newdata, int index)
{
    AppAssert(index < static_cast<int>(this->array.size()) && index >= 0);

    this->array[index] = std::move(newdata);

    if (this->shadow[index] == 0)
    {
        this->shadow[index] = 1;
        ++numused;
        RebuildFreeList();
    }
}


template <class T>
template<typename U>
std::enable_if_t<std::is_pointer_v<U>> FastDArray<T>::EmptyAndDelete()
{
    const int currentSize = static_cast<int>(this->array.size());
    for (int i = 0; i < currentSize; ++i)
    {
        if (this->shadow[i] == 1)
        {
            delete this->array[i];
            this->array[i] = nullptr;
        }
    }

    Empty();
}


template <class T>
template<typename U>
std::enable_if_t<std::is_pointer_v<U>> FastDArray<T>::EmptyAndDeleteArray()
{
    const int currentSize = static_cast<int>(this->array.size());
    for (int i = 0; i < currentSize; ++i)
    {
        if (this->shadow[i] == 1)
        {
            delete[] this->array[i];
            this->array[i] = nullptr;
        }
    }

    Empty();
}


template <class T>
void FastDArray<T>::Empty() noexcept
{
    freelist.clear();
    firstfree = -1;
    numused = 0;

    DArray<T>::Empty();
}


template <class T>
void FastDArray<T>::MarkUsed(int index)
{
    AppAssert(index < static_cast<int>(this->array.size()) && index >= 0);
    AppAssert(this->shadow[index] == 0);
    
    this->shadow[index] = 1;
    ++numused;
    RebuildFreeList();
}


template <class T>
void FastDArray<T>::RemoveData(int index)
{
    AppAssert(index < static_cast<int>(this->array.size()) && index >= 0);
    AppAssert(this->shadow[index] != 0);
    
    --numused;
    this->shadow[index] = 0;
    freelist[index] = firstfree;
    firstfree = index;
}


template <class T>
T* FastDArray<T>::GetPointer()
{
    return GetPointer(GetNextFree());
}


template <class T>
T* FastDArray<T>::GetPointer(int index)
{
    return DArray<T>::GetPointer(index);
}


template <class T>
int FastDArray<T>::GetNextFree()
{
    if (firstfree == -1)
    {
        Grow();
    }

    const int freeslot = firstfree;
    const int nextfree = freelist[freeslot];
    
    if (this->shadow[freeslot] == 0)
    {
        ++numused;
    }

    this->shadow[freeslot] = 1;
    freelist[freeslot] = -2;
    firstfree = nextfree;

    return freeslot;
}


template <class T>
constexpr int FastDArray<T>::NumUsed() const noexcept
{
    return numused;
}


template <class T>
bool FastDArray<T>::ValidateFreeList() const
{
    const int currentSize = static_cast<int>(this->array.size());
    
	//
    // Check all used slots have freelist[i] == -2

    for (int i = 0; i < currentSize; ++i)
    {
        if (this->shadow[i] == 1)
        {
            if (freelist[i] != -2)
                return false;
        }
    }
    
	//
    // Check free list is valid

    int visited = 0;
    int current = firstfree;
    std::vector<bool> seen(currentSize, false);
    
    while (current != -1 && visited < currentSize)
    {
        if (current < 0 || current >= currentSize)
            return false;  // Invalid index
            
        if (seen[current])
            return false;  // Cycle detected
            
        if (this->shadow[current] != 0)
            return false;  // Free slot is marked as used
            
        seen[current] = true;
        current = freelist[current];
        ++visited;
    }
    
    int actualFree = 0;
    for (int i = 0; i < currentSize; ++i)
    {
        if (this->shadow[i] == 0)
            ++actualFree;
    }
    
    return (visited == actualFree) && (numused + actualFree == currentSize);
}