#include "Paths.h"
#include <windows.h>
#include "Util.h"
#include "Error.h"

namespace
{
	bool s_initialized = false;
	char s_binDataDir[FILE_MAX_PATH];
	char s_srcDataDir[FILE_MAX_PATH];


	void Init()
	{
		char path[FILE_MAX_PATH];
		GetCurrentDirectoryA(ARRAY_SIZE(path), path);
		strcat_s(path, "\\");

		while (true)
		{
			sprintf_s(s_binDataDir, "%sbin\\data", path);
			if (GetFileAttributesA(s_binDataDir) != INVALID_FILE_ATTRIBUTES)
			{
				// Set src data dir as well.  This only needs to exist for editing.
				sprintf_s(s_srcDataDir, "%sdata", path);
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


const char* Paths::GetBinDataDir()
{
	if (!s_initialized)
		Init();
	return s_binDataDir;
}

const char* Paths::GetSrcDataDir()
{
	if (!s_initialized)
		Init();
	return s_srcDataDir;
}