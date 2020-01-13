#include "Paths.h"
#include <windows.h>
#include "Util.h"
#include "Error.h"

namespace
{
	bool s_initialized = false;
	char s_srcDataDir[FILE_MAX_PATH];
	char s_binDataDir[FILE_MAX_PATH];

	void Init()
	{
		char path[FILE_MAX_PATH];
		GetCurrentDirectoryA(ARRAY_SIZE(path), path);
		strcat_s(path, "\\");

		while (true)
		{
			char subPath[FILE_MAX_PATH];
			sprintf_s(subPath, "%sdata", path);
			if (GetFileAttributesA(subPath) != INVALID_FILE_ATTRIBUTES)
			{
				if (strstr(subPath, "bin\\data") != nullptr)
				{
					strcpy_s(s_binDataDir, subPath);
				}
				else
				{
					strcpy_s(s_srcDataDir, subPath);
					break;
				}
			}

			bool foundParentDir = false;
			for (int i = (int)strlen(path) - 2; i >= 0; --i)
			{
				if (path[i] == '/' || path[i] == '\\')
				{
					path[i + 1] = 0;
					foundParentDir = true;
					break;
				}
			}

			if (!foundParentDir)
			{
				Error("Failed to locate data directories.");
			}
		}

		s_initialized = true;
	}
}

const char* Paths::GetSrcDataDir()
{
	if (!s_initialized)
		Init();
	return s_srcDataDir;
}

const char* Paths::GetBinDataDir()
{
	if (!s_initialized)
		Init();
	return s_binDataDir;
}

const char* Paths::GetExtension(const char* filename)
{
	int len = (int)strlen(filename);

	int i = len - 1;
	while (i > 0 && filename[i] != '.')
	{
		--i;
	}

	return (i == 0) ? nullptr : &filename[i + 1];
}

void Paths::GetFilenameNoExtension(const char* path, char* outFilename, int maxNameLen)
{
	int pathLen = (int)strlen(path);

	int startPos = pathLen - 1;
	for (; startPos > 0; --startPos)
	{
		if (path[startPos] == '/' || path[startPos] == '\\')
		{
			startPos += 1; // Skip over the slash
			break;
		}
	}

	int endPos = startPos;
	for (; endPos < pathLen; ++endPos)
	{
		if (path[endPos] == '.')
		{
			break;
		}
	}

	strncpy_s(outFilename, maxNameLen, path + startPos, (endPos - startPos));
}

void Paths::ForEachFile(const char* searchPattern, bool includeSubDirs, FileCallback callback, void* pUserData)
{
	WIN32_FIND_DATAA findData;

	HANDLE hFind = FindFirstFileA(searchPattern, &findData);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			callback(findData.cFileName, true, pUserData);
			if (includeSubDirs)
				ForEachFile(findData.cFileName, true, callback, pUserData);
		}
		else
		{
			callback(findData.cFileName, false, pUserData);
		}
	} 
	while (FindNextFileA(hFind, &findData) != 0);

	FindClose(hFind);
}

void Paths::CreateDirectoryTreeForFile(const std::string& filename)
{
	int endPos = -1;
	while ((endPos = (int)filename.find_first_of("/\\", endPos + 1)) != std::string::npos)
	{
		std::string dir = filename.substr(0, endPos);
		CreateDirectoryA(dir.c_str(), nullptr);
	}
}