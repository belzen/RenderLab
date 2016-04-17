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
		m_pPos = m_pMemory;
		m_availSize = m_size;
	}

private:
	void* m_pMemory;
	void* m_pPos;
	size_t m_size;
	size_t m_availSize;
};
