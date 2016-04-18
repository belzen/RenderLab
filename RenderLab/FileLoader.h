#pragma once

#include "json/json.h"

namespace FileLoader
{
	// todo: async load
	bool Load(const char* filename, char** ppOutData, uint* pOutDataSize);

	bool LoadJson(const char* filename, Json::Value& rOutJsonRoot);
}
