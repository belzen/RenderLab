#include "Precompiled.h"
#include "FileLoader.h"

bool FileLoader::Load(const char* filename, char** ppOutData, uint* pOutDataSize)
{
	FILE* file;
	fopen_s(&file, filename, "rb");

	fseek(file, 0, SEEK_END);
	int fileSize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* pFileData = new char[fileSize];
	fread(pFileData, sizeof(char), fileSize, file);
	fclose(file);

	*ppOutData = pFileData;
	*pOutDataSize = fileSize;

	return true;
}

bool FileLoader::LoadJson(const char* filename, Json::Value& rOutJsonRoot)
{
	Json::Value jRoot;

	char* fileData;
	uint fileSize;
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