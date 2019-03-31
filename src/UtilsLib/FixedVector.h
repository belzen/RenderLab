#pragma once

// Fixed size vector that maintains size state internally.
// Indices outside of the current size are inaccessible.
template<typename T_object, uint T_kCapacity>
class FixedVector
{
public:
	FixedVector()
	{
		memset(m_objects, 0, sizeof(m_objects));
		m_size = 0;
	}

	T_object& get(uint idx)
	{
		assert(idx < m_size);
		return m_objects[idx];
	}

	const T_object& get(uint idx) const
	{
		assert(idx < m_size);
		return m_objects[idx];
	}

	T_object& operator[] (uint idx)
	{
		assert(idx < m_size);
		return m_objects[idx];
	}

	const T_object& operator[] (uint idx) const
	{
		assert(idx < m_size);
		return m_objects[idx];
	}

	// Increment the size of the array and return a reference to the new entry.
	T_object& push()
	{
		assert(m_size < T_kCapacity);
		return m_objects[m_size++];
	}
	
	T_object& pushSafe()
	{
		assert(m_size < T_kCapacity);
		uint i = InterlockedIncrement(&m_size) - 1;
		return m_objects[i];
	}

	// Increment the size of the array and return a reference to the new entry.
	T_object& push(const T_object& other)
	{
		return (push() = other);
	}

	T_object& pushSafe(const T_object& other)
	{
		return (pushSafe() = other);
	}

	const T_object& pop()
	{
		return m_objects[--m_size];
	}

	const T_object* getData() const
	{
		return m_objects;
	}

	void clear()
	{
		m_size = 0;
	}

	uint size() const
	{
		return m_size;
	}

	uint capacity() const
	{
		return T_kCapacity;
	}

	//////////////////////////////////////////////////////////////////////////
	// Range iterator
	T_object* begin()
	{
		return &m_objects[0];
	}

	T_object* end()
	{
		return &m_objects[m_size];
	}

	void eraseFast(uint nIndex)
	{
		m_objects[nIndex] = m_objects[m_size - 1];
		--m_size;
	}

private:
	T_object m_objects[T_kCapacity];
	uint m_size;
};