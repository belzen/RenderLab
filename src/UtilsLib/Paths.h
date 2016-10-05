#pragma once

#define FILE_MAX_PATH 256

namespace Paths
{
	const char* GetDataDir();

	const char* GetExtension(const char* filename);

	void GetFilenameNoExtension(const char* path, char* outFilename, int maxNameLen);
}