#pragma once
#include "../Types.h"

// String caching utility.
// Useful for simplifying string comparisons and memory usage of common strings.
// When provided with a c-string, the CachedString constructor either adds
// a duplicate of the string to the cache or returns an existing entry for the string.
class CachedString
{
public:
	CachedString();
	CachedString(const char* str);

	bool operator==(const CachedString& rOther) const;
	explicit operator bool() const;

	uint getHash() const;
	const char* getString() const;

private:
	const char* m_str;
	uint m_hash;
};

//////////////////////////////////////////////////////////////////////////
inline bool CachedString::operator==(const CachedString& rOther) const
{
	return m_str == rOther.m_str;
}

inline CachedString::operator bool() const
{
	return !!m_str;
}

inline uint CachedString::getHash() const
{
	return m_hash;
}

inline const char* CachedString::getString() const
{
	return m_str;
}
