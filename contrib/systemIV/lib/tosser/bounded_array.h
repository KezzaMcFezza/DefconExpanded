#ifndef INCLUDED_BOUNDED_ARRAY_H
#define INCLUDED_BOUNDED_ARRAY_H

#include <vector>
#include <algorithm>

#include "lib/debug/debug_utils.h"

// ****************************************************************************
// Class BoundedArray
// ****************************************************************************

template <class T>
class BoundedArray
{
protected:
	std::vector<T>  m_data;

public:
	BoundedArray() = default;
	BoundedArray( BoundedArray const &_otherArray ) = default;
	BoundedArray( BoundedArray &&_otherArray ) noexcept = default;
	explicit BoundedArray(size_t _numElements ) : m_data(_numElements) {}
	~BoundedArray() = default;

	BoundedArray &operator=( BoundedArray const &_otherArray ) = default;
	BoundedArray &operator=( BoundedArray &&_otherArray ) noexcept = default;

	void Empty() { m_data.clear(); }
		
	void Initialise( size_t _numElements )             { m_data.resize(_numElements); }
	void Initialise( BoundedArray const &_otherArray ) { m_data = _otherArray.m_data; }

	inline T &operator[](size_t _index) {
		AppAssert(_index < m_data.size());
		return m_data[_index];
	}
	
	inline const T &operator[](size_t _index) const {
		AppAssert(_index < m_data.size());
		return m_data[_index];
	}

	inline size_t Size() const noexcept { return m_data.size(); }
	
	void SetAll(T const &_value) {
		std::fill(m_data.begin(), m_data.end(), _value);
	}
	
	//
	// Iterators

	auto begin()  noexcept { return m_data.begin(); }
	auto end()    noexcept { return m_data.end(); }
	auto begin()  const noexcept { return m_data.begin(); }
	auto end()    const noexcept { return m_data.end(); }
	auto cbegin() const noexcept { return m_data.cbegin(); }
	auto cend()   const noexcept { return m_data.cend(); }
};

// ****************************************************************************
// Specialization for bool, std::vector<bool> is doesnt return real references

template <>
class BoundedArray<bool>
{
protected:
	std::vector<unsigned char> m_data;

public:
	BoundedArray() = default;
	BoundedArray( BoundedArray const &_otherArray ) = default;
	BoundedArray( BoundedArray &&_otherArray ) noexcept = default;
	explicit BoundedArray( size_t _numElements ) : m_data(_numElements, 0) {}
	~BoundedArray() = default;

	BoundedArray &operator=( BoundedArray const &_otherArray ) = default;
	BoundedArray &operator=( BoundedArray &&_otherArray ) noexcept = default;

	void Empty() { m_data.clear(); }
		
	void Initialise( size_t _numElements )             { m_data.resize(_numElements, 0); }
	void Initialise( BoundedArray const &_otherArray ) { m_data = _otherArray.m_data; }

	bool operator[](size_t _index) const {
		AppAssert(_index < m_data.size());
		return m_data[_index] != 0;
	}
	
	class BoolProxy {
		std::vector<unsigned char> &m_vec;
		size_t m_idx;
	public:
		BoolProxy(std::vector<unsigned char> &vec, size_t idx) : m_vec(vec), m_idx(idx) {}
		operator bool() const { return m_vec[m_idx] != 0; }
		BoolProxy &operator=(bool val) { m_vec[m_idx] = val ? 1 : 0; return *this; }
		BoolProxy &operator=(const BoolProxy &other) { m_vec[m_idx] = other.m_vec[other.m_idx]; return *this; }
	};
	
	BoolProxy operator[](size_t _index) {
		AppAssert(_index < m_data.size());
		return BoolProxy(m_data, _index);
	}

	size_t Size() const noexcept { return m_data.size(); }
	
	void SetAll(bool const &_value) {
		unsigned char val = _value ? 1 : 0;
		std::fill(m_data.begin(), m_data.end(), val);
	}
};

#endif