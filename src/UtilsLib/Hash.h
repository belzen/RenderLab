#pragma once

#include "../Types.h"

struct SHA1Hash
{
	static void Calculate(char* pData, uint dataSize, SHA1Hash& rOutHash);

	uchar data[20];
};
