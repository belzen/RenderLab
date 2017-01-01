#include "StringCache.h"
#include "../Types.h"
#include "Hash.h"
#include <map>

namespace
{
	// TODO: Use large chunks of memory to store strings contiguously instead
	//		of separate allocations for each string.
	typedef std::map<uint, const char*> StringMap;
	StringMap s_stringCache;
}

CachedString::CachedString()
	: m_str(nullptr)
	, m_hash(0)
{

}

CachedString::CachedString(const char* str)
{
	m_hash = Hashing::HashString(str);
	StringMap::iterator iter = s_stringCache.lower_bound(m_hash);
	if (iter != s_stringCache.end() && iter->first == m_hash)
	{
		m_str = iter->second;
	}
	else
	{
		m_str = _strdup(str);
		StringMap::value_type val(m_hash, m_str);
		s_stringCache.insert(iter, val);
	}
}