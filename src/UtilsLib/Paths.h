#pragma once

#define FILE_MAX_PATH 256

namespace Paths
{
	const char* GetDataDir();

	const char* GetExtension(const char* filename);

	void GetFilenameNoExtension(const char* path, char* outFilename, int maxNameLen);

	typedef void (*FileCallback)(const char* filename, bool isDirectory, void* pUserData);
	void ForEachFile(const char* searchPattern, bool includeSubDirs, FileCallback callback, void* pUserData);
}
