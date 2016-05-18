#pragma once

#include "../Types.h"

namespace Hashing
{
	typedef void SHA1HashState;

	struct SHA1
	{
		static SHA1HashState* Begin();
		static SHA1HashState* Begin(char* pData, uint dataSize);
		static void Update(SHA1HashState* pState, char* pData, uint dataSize);
		static void Finish(SHA1HashState* pState, SHA1& rOutHash);

		static void Calculate(char* pData, uint dataSize, SHA1& rOutHash);

		uchar data[20];
	};

	typedef uint StringHash;

	StringHash HashString(const char* str);
}
