#pragma once

#include "../Types.h"
#include <vcruntime_string.h>

namespace Hashing
{
	typedef void SHA1HashState;

	struct SHA1
	{
		static SHA1HashState* Begin();
		static SHA1HashState* Begin(const void* pData, uint dataSize);
		static void Update(SHA1HashState* pState, const void* pData, uint dataSize);
		static void Finish(SHA1HashState* pState, SHA1& rOutHash);

		uchar data[20];
	
		bool operator < (const SHA1& other) const
		{
			return memcmp(data, other.data, sizeof(data)) < 0;
		}
	};

	typedef uint StringHash;

	StringHash HashString(const char* str);
}
