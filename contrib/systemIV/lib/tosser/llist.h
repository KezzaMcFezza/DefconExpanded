#ifndef _included_llist_h
#define _included_llist_h

#include <deque>
#include <algorithm>

//=================================================================
// Linked list object
// Modernized to use std::deque internally while maintaining backward-compatible API
// 
// Use : A dynamically sized list of data
// NOT sorted in any way - new data is simply added on at the end
// Indexes of data are not constant during removals
//=================================================================

template <class T>
class LListItem
{
public:
	T m_data;
	LListItem* m_next;
	LListItem* m_previous;
	
	LListItem() : m_next(nullptr), m_previous(nullptr) {}
	~LListItem() {}
};


template <class T>
class LList
{
protected:
	std::deque<T> m_data;

public:
	LList() = default;
	~LList() = default;
	
	LList( const LList<T>& source ) : m_data(source.m_data) {}
	
	LList( LList<T>&& ) noexcept = default;
	
	LList& operator=( const LList<T>& source ) {
		if (this != &source) {
			m_data = source.m_data;
		}
		return *this;
	}
	
	LList& operator=( LList<T>&& ) noexcept = default;
	
	inline void PutData( const T& newdata ) {
		m_data.push_back(newdata);
	}
	
	void PutDataAtEnd( const T& newdata ) {
		m_data.push_back(newdata);
	}
	
	void PutDataAtStart( const T& newdata ) {
		m_data.push_front(newdata);
	}
	
	void PutDataAtIndex( const T& newdata, int index ) {
		if (index <= 0) {
			PutDataAtStart(newdata);
		} else if (index >= static_cast<int>(m_data.size())) {
			PutDataAtEnd(newdata);
		} else {
			m_data.insert(m_data.begin() + index, newdata);
		}
	}
	
	inline T GetData( int index ) const {
		if (!ValidIndex(index)) {
			return T();
		}
		return m_data[index];
	}
	
	inline T* GetPointer( int index ) const {
		if (!ValidIndex(index)) {
			return nullptr;
		}
		return const_cast<T*>(&m_data[index]);
	}
	
	void RemoveData( int index ) {
		if (ValidIndex(index)) {
			m_data.erase(m_data.begin() + index);
		}
	}
	
	inline void RemoveDataAtEnd() {
		if (!m_data.empty()) {
			m_data.pop_back();
		}
	}
	
	int FindData( const T& data ) {
		auto it = std::find(m_data.begin(), m_data.end(), data);
		if (it != m_data.end()) {
			return static_cast<int>(std::distance(m_data.begin(), it));
		}
		return -1;
	}
	
	inline int Size() const {
		return static_cast<int>(m_data.size());
	}
	
	inline bool ValidIndex( int index ) const {
		return (index >= 0 && index < static_cast<int>(m_data.size()));
	}
	
	void Empty() {
		m_data.clear();
	}
	
	template<typename U = T>
	typename std::enable_if<std::is_pointer<U>::value>::type
	EmptyAndDelete() {
		for (auto ptr : m_data) {
			delete ptr;
		}
		m_data.clear();
	}
	
	template<typename U = T>
	typename std::enable_if<std::is_pointer<U>::value>::type
	EmptyAndDeleteArray() {
		for (auto ptr : m_data) {
			delete[] ptr;
		}
		m_data.clear();
	}
	
	inline T operator[](int index) {
		return GetData(index);
	}
	
	//
	// Iterators

	auto begin() { return m_data.begin(); }
	auto end() { return m_data.end(); }
	auto begin() const { return m_data.begin(); }
	auto end() const { return m_data.end(); }
	auto cbegin() const { return m_data.cbegin(); }
	auto cend() const { return m_data.cend(); }
};

#endif