#include "Precompiled.h"
#include "FileLoader.h"

bool FileLoader::Load(const char* filename, void** ppOutData, uint* pOutDataSize)
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