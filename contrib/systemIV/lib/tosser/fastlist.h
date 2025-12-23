#ifndef _included_fastlist_h
#define _included_fastlist_h

#include <vector>
#include <algorithm>
#include <string_view>
#include <type_traits>

/*
 *  Fast List container class
 *
 *  Essentially a linked list, automatically grows and shrinks
 *  Indexes of elements are not maintained
 *
 *  Fast access to data at any index: O(1)
 *  Slower insertion and deletion in the middle: O(n)
 *  Fast insertion at end (standard PutData behaviour): O(1)
 *  Fast removal with swap: O(1)
 *
 */

template <class T>
class FastList
{
protected:
    std::vector<T> m_data;

public:
    FastList() = default;
    ~FastList() = default;
    
    FastList           ( const FastList& ) = delete;
    FastList& operator=( const FastList& ) = delete;
    
    FastList           ( FastList&& ) noexcept = default;
    FastList& operator=( FastList&& ) noexcept = default;
    
    void Copy(const FastList& source) {
        m_data = source.m_data;
    }
    
    [[nodiscard]] int PutData( const T& newdata ) {
        m_data.push_back(newdata);
        return static_cast<int>(m_data.size()) - 1;
    }
    
    // Move version
    [[nodiscard]] int PutData( T&& newdata ) {
        m_data.push_back(std::move(newdata));
        return static_cast<int>(m_data.size()) - 1;
    }
    
    template<typename... Args>
    [[nodiscard]] int EmplaceData( Args&&... args ) {
        m_data.emplace_back(std::forward<Args>(args)...);
        return static_cast<int>(m_data.size()) - 1;
    }
    
    [[nodiscard]] int PutDataAtEnd( const T& newdata ) {
        return PutData(newdata);
    }
    
    [[nodiscard]] int PutDataAtEnd( T&& newdata ) {
        return PutData(std::move(newdata));
    }
    
    [[nodiscard]] int PutDataAtStart( const T& newdata ) {
        m_data.insert(m_data.begin(), newdata);
        return 0;
    }
    
    [[nodiscard]] int PutDataAtStart( T&& newdata ) {
        m_data.insert(m_data.begin(), std::move(newdata));
        return 0;
    }
    
    [[nodiscard]] int PutDataAtIndex( const T& newdata, int index ) {
        AppDebugAssert(index >= 0 && index <= static_cast<int>(m_data.size()));
        m_data.insert(m_data.begin() + index, newdata);
        return index;
    }
    
    [[nodiscard]] int PutDataAtIndex( T&& newdata, int index ) {
        AppDebugAssert(index >= 0 && index <= static_cast<int>(m_data.size()));
        m_data.insert(m_data.begin() + index, std::move(newdata));
        return index;
    }
    
    [[nodiscard]] T GetData( int index ) const {
        AppDebugAssert(index >= 0);
        AppDebugAssert(index < static_cast<int>(m_data.size()));
        return m_data[index];
    }
    
    // Non const version
    [[nodiscard]] T& GetDataRef( int index ) {
        AppDebugAssert(index >= 0);
        AppDebugAssert(index < static_cast<int>(m_data.size()));
        return m_data[index];
    }
    
    // Const version
    [[nodiscard]] const T& GetDataRef( int index ) const {
        AppDebugAssert(index >= 0);
        AppDebugAssert(index < static_cast<int>(m_data.size()));
        return m_data[index];
    }
    
    [[nodiscard]] T* GetPointer( int index ) const {
        AppDebugAssert(index >= 0);
        AppDebugAssert(index < static_cast<int>(m_data.size()));
        return const_cast<T*>(&m_data[index]);
    }
    
    void RemoveData( int index ) {
        AppAssert(0 <= index && index < static_cast<int>(m_data.size()));
        m_data.erase(m_data.begin() + index);
    }
    
	//
    // Swaps with last element then removes

    void RemoveDataWithSwap( int index ) {
        AppAssert(0 <= index && index < static_cast<int>(m_data.size()));
        const int lastIndex = static_cast<int>(m_data.size()) - 1;
        if (index != lastIndex) {
            m_data[index] = std::move(m_data[lastIndex]);
        }
        m_data.pop_back();
    }
    
    [[nodiscard]] int FindData( const T& data ) const {
        auto it = std::find(m_data.begin(), m_data.end(), data);
        if (it != m_data.end()) {
            return static_cast<int>(std::distance(m_data.begin(), it));
        }
        return -1;
    }
    
    [[nodiscard]] constexpr int Size() const noexcept {
        return static_cast<int>(m_data.size());
    }
    
    void EnsureSpace( int capacity ) {
        if (capacity > static_cast<int>(m_data.capacity())) {
            m_data.reserve(capacity);
        }
    }
    
    void Reserve( int capacity ) {
        if (capacity > 0) {
            m_data.reserve(capacity);
        }
    }
    
    [[nodiscard]] constexpr int Capacity() const noexcept {
        return static_cast<int>(m_data.capacity());
    }
    
    void Resize( int size ) {
        AppDebugAssert(size >= 0);
        m_data.resize(size);
    }
    
    [[nodiscard]] constexpr bool ValidIndex( int index ) const noexcept {
        return (index >= 0 && index < static_cast<int>(m_data.size()));
    }
    
    [[nodiscard]] constexpr bool Empty() const noexcept {
        return m_data.empty();
    }
    
    void Append( const FastList<T>& other ) {
        m_data.insert(m_data.end(), other.m_data.begin(), other.m_data.end());
    }
    
    void Swap( FastList<T>& other ) noexcept {
        m_data.swap(other.m_data);
    }
    
    void Clear() noexcept {
        m_data.clear();
    }
    
    void Empty() noexcept {
        Clear();
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>> EmptyAndDelete() {
        for (auto ptr : m_data) {
            delete ptr;
        }
        m_data.clear();
    }
    
    template<typename U = T>
    std::enable_if_t<std::is_pointer_v<U>> EmptyAndDeleteArray() {
        for (auto ptr : m_data) {
            delete[] ptr;
        }
        m_data.clear();
    }
    
	//
    // Safer access with bounds checking

    [[nodiscard]] T& operator[]( int index ) const {
        AppDebugAssert(index >= 0 && index < static_cast<int>(m_data.size()));
        return const_cast<T&>(m_data[index]);
    }
    
    [[nodiscard]] bool operator==( const FastList<T>& other ) const noexcept {
        return m_data == other.m_data;
    }
    
    [[nodiscard]] bool operator!=( const FastList<T>& other ) const noexcept {
        return m_data != other.m_data;
    }
    
    [[nodiscard]] std::vector<T>& GetVector() noexcept {
        return m_data;
    }
    
    [[nodiscard]] const std::vector<T>& GetVector() const noexcept {
        return m_data;
    }
    
	//
    // Iterators

    [[nodiscard]] auto begin() noexcept { return m_data.begin(); }
    [[nodiscard]] auto end() noexcept { return m_data.end(); }
    [[nodiscard]] auto begin() const noexcept { return m_data.begin(); }
    [[nodiscard]] auto end() const noexcept { return m_data.end(); }
    [[nodiscard]] auto cbegin() const noexcept { return m_data.cbegin(); }
    [[nodiscard]] auto cend() const noexcept { return m_data.cend(); }
    [[nodiscard]] auto rbegin() noexcept { return m_data.rbegin(); }
    [[nodiscard]] auto rend() noexcept { return m_data.rend(); }
    [[nodiscard]] auto rbegin() const noexcept { return m_data.rbegin(); }
    [[nodiscard]] auto rend() const noexcept { return m_data.rend(); }
};


template <class T>
class CopyableFastList : public FastList<T>
{
public:
    CopyableFastList() = default;
    
    CopyableFastList( const CopyableFastList& source ) {
        this->Copy(source);
    }
    
    CopyableFastList( CopyableFastList&& other ) noexcept = default;
    
    CopyableFastList& operator=( const CopyableFastList& source ) {
        if (this != &source) {
            this->Copy(source);
        }
        return *this;
    }
    
    CopyableFastList& operator=( CopyableFastList&& other ) noexcept = default;
};

template<class T, class P>
void CopyMatching( const FastList<T>& source, FastList<T>& destination, P predicate ) {
    destination.Reserve(source.Size());
    for (int i = 0; i < source.Size(); ++i) {
        const T& x = source[i];
        if (predicate(x)) {
            destination.PutData(x);
        }
    }
}

template<class T, class P>
void MoveMatching( FastList<T>& source, FastList<T>& destination, P predicate ) {
    int writeIndex = 0;
    for (int readIndex = 0; readIndex < source.Size(); ++readIndex) {
        T& x = source[readIndex];
        if (predicate(x)) {
            destination.PutData(std::move(x));
        } else {
            if (readIndex != writeIndex) {
                source[writeIndex] = std::move(x);
            }
            ++writeIndex;
        }
    }
    
    if (writeIndex != source.Size()) source.Resize(writeIndex);
}

template<class T, class P>
void MoveMatchingToFront( FastList<T>& source, FastList<T>& destination, P predicate ) {
    FastList<T> matched;
    
    MoveMatching(source, matched, predicate);
    if (matched.Size() > 0) {
        matched.Swap(destination);
        for (int i = 0; i < matched.Size(); ++i) {
            destination.PutData(std::move(matched[i]));
        }
    }
}

template<class T, class P>
[[nodiscard]] int CountMatching( const FastList<T>& source, P predicate ) {
    int count = 0;
    for (int readIndex = 0; readIndex < source.Size(); ++readIndex) {
        if (predicate(source[readIndex])) ++count;
    }
    return count;
}

template<class T, class P>
void RemoveMatching( FastList<T>& source, P predicate ) {
    int writeIndex = 0;
    for (int readIndex = 0; readIndex < source.Size(); ++readIndex) {
        T& x = source[readIndex];
        if (!predicate(x)) {
            if (readIndex != writeIndex) {
                source[writeIndex] = std::move(x);
            }
            ++writeIndex;
        }
    }
    
    if (writeIndex != source.Size()) source.Resize(writeIndex);
}

template<class T>
void Sort( FastList<T>& list ) {
    std::sort(list.begin(), list.end());
}

template<class T, class Compare>
void Sort( FastList<T>& list, Compare comp ) {
    std::sort(list.begin(), list.end(), comp);
}

#endif