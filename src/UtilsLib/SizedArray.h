#pragma once

// Fixed size array that maintains size state internally.
// Indices outside of the current size are inaccessible.
template<typename T_object, uint T_arraySize>
class SizedArray
{
public:
	SizedArray()
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
		assert(index < m_size);
		return m_objects[idx];
	}

	// Increment the size of the array and return a reference to the new entry.
	T_object& push()
	{
		assert(m_size < T_arraySize);
		return m_objects[m_size++];
	}
	
	T_object& pushSafe()
	{
		assert(m_size < T_arraySize);
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

	void clear()
	{
		m_size = 0;
	}

	uint size()
	{
		return m_size;
	}

private:
	T_object m_objects[T_arraySize];
	uint m_size;
};