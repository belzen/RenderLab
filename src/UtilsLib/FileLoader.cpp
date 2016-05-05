#include "FileLoader.h"
#include <fstream>
#include "json/json.h"

bool FileLoader::Load(const char* filename, char** ppOutData, unsigned int* pOutDataSize)
{
	struct stat fileStats;
	int res = stat(filename, &fileStats);
	if (res != 0)
		return false;

	std::ifstream file(filename, std::ios::binary);
	if (!file.is_open())
		return false;

	unsigned int fileSize = fileStats.st_size;
	char* pFileData = new char[fileSize];
	file.read(pFileData, fileSize);
	
	*ppOutData = pFileData;
	*pOutDataSize = fileSize;

	return true;
}

bool FileLoader::LoadJson(const char* filename, Json::Value& rOutJsonRoot)
{
	Json::Value jRoot;

	char* fileData;
	unsigned int fileSize;
	if (!FileLoader::Load(filename, &fileData, &fileSize))
	{
		rOutJsonRoot = Json::nullValue;
		return false;
	}

	Json::Reader jsonReader;
	jsonReader.parse(fileData, fileData + fileSize, rOutJsonRoot, false);
	delete fileData;

	return true;
}