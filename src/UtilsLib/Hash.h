#pragma once

#include "../Types.h"

typedef void SHA1HashState;

struct SHA1Hash
{
	static SHA1HashState* Begin();
	static SHA1HashState* Begin(char* pData, uint dataSize);
	static void Update(SHA1HashState* pState, char* pData, uint dataSize);
	static void Finish(SHA1HashState* pState, SHA1Hash& rOutHash);

	static void Calculate(char* pData, uint dataSize, SHA1Hash& rOutHash);

	uchar data[20];
};
