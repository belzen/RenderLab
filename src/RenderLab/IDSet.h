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

	uint getMaxIds() const
	{
		return T_nMaxIds;
	}

private:
	uint m_nextId;
	uint m_freeIds[T_nMaxIds];
	uint m_numFree;
};

class IdSetDynamic
{
public:
	IdSetDynamic() : m_nextId(1), m_numFree(0), m_pFreeIds(nullptr), m_maxIds(0)
	{

	}

	~IdSetDynamic()
	{
		SAFE_DELETE(m_pFreeIds);
	}

	void init(uint maxIds)
	{
		m_maxIds = maxIds;
		m_pFreeIds = new uint[m_maxIds];
	}

	uint allocId()
	{
		if (m_numFree > 0)
			return m_pFreeIds[--m_numFree];

		if (m_nextId >= m_maxIds)
			assert(false);

		return m_nextId++;
	}

	void releaseId(uint id)
	{
		m_pFreeIds[m_numFree++] = id;
	}

	uint getMaxIds() const
	{
		return m_maxIds;
	}

private:
	uint m_nextId;
	uint* m_pFreeIds;
	uint m_numFree;
	uint m_maxIds;
};