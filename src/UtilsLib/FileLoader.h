#pragma once

#include "json/json-forwards.h"

namespace FileLoader
{
	// todo: async load
	bool Load(const char* filename, char** ppOutData, unsigned int* pOutDataSize);

	bool LoadJson(const char* filename, Json::Value& rOutJsonRoot);
}
