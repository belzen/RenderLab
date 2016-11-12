#pragma once

template<size_t AllocatorSizeT>
class LinearAllocator
{
public:
	inline LinearAllocator::LinearAllocator()
		: m_size(AllocatorSizeT)
		, m_availSize(AllocatorSizeT)
	{
		m_pMemory = m_pPos = new char[AllocatorSizeT];
		memset(m_pMemory, 0, m_size);
	}

	inline ~LinearAllocator()
	{
		SAFE_DELETE(m_pMemory);
	}

	inline void* Alloc(uint size, uint alignment = 4)
	{
		assert(m_availSize - size - alignment > 0);

		std::align(alignment, size, m_pPos, m_availSize);

		void* pResult = m_pPos;
		m_pPos = (char*)m_pPos + size;
		m_availSize -= size;

		return pResult;
	}

	inline void Reset()
	{
		memset(m_pMemory, 0, (m_size - m_availSize));
		m_pPos = m_pMemory;
		m_availSize = m_size;
	}

private:
	void* m_pMemory;
	void* m_pPos;
	size_t m_size;
	size_t m_availSize;
};

template<typename DataTypeT, uint kCapacityT>
class StructLinearAllocator
{
public:
	inline StructLinearAllocator::StructLinearAllocator()
		: m_size(0)
	{
		memset(m_data, 0, sizeof(DataTypeT) * kCapacityT);
	}

	inline ~StructLinearAllocator()
	{
	}

	inline DataTypeT* Alloc()
	{
		uint elem = InterlockedIncrement(&m_size);
		return &m_data[elem];
	}

	inline void Reset()
	{
		memset(m_data, 0, sizeof(DataTypeT) * m_size);
		m_size = 0;
	}

private:
	DataTypeT m_data[kCapacityT];
	uint m_size;
};

