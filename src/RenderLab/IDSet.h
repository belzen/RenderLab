#pragma once

template <uint T_nMaxIds>
class IdSet
{
public:
	IdSet() : m_nextId(1), m_numFree(0)
	{

	}

	uint allocId()
	{
		if (m_numFree > 0)
			return m_freeIds[--m_numFree];

		if (m_nextId >= T_nMaxIds)
			assert(false);

		return m_nextId++;
	}

	void releaseId(uint id)
	{
		m_freeIds[m_numFree++] = id;
	}

private:
	uint m_nextId;
	uint m_freeIds[T_nMaxIds];
	uint m_numFree;
};