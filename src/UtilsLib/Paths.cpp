#include "Paths.h"
#include <windows.h>
#include "Util.h"
#include "Error.h"

namespace
{
	bool s_initialized = false;
	char s_dataDir[FILE_MAX_PATH];

	void Init()
	{
		char path[FILE_MAX_PATH];
		GetCurrentDirectoryA(ARRAY_SIZE(path), path);
		strcat_s(path, "\\");

		while (true)
		{
			sprintf_s(s_dataDir, "%sdata", path);
			if (GetFileAttributesA(s_dataDir) != INVALID_FILE_ATTRIBUTES)
			{
				break;
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


const char* Paths::GetDataDir()
{
	if (!s_initialized)
		Init();
	return s_dataDir;
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