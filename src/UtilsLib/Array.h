#pragma once

template<typename T_object, uint T_kCapacity>
class Array
{
public:
	Array()
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

	void assign(uint elem, const T_object& rData)
	{
		m_objects[elem] = rData;
		m_size = std::max(elem + 1, m_size);
	}

	void push(const T_object& rData)
	{
		m_objects[m_size] = rData;
		++m_size;
	}

	uint size() const
	{
		return m_size;
	}

	uint capacity() const
	{
		return T_kCapacity;
	}

private:
	T_object m_objects[T_kCapacity];
	uint m_size;
};
