#include "Precompiled.h"
#include "FileWatcher.h"
#include <windows.h>
#include <assert.h>
#include <thread>
#include <fileapi.h>

static std::thread s_thread;

static void fileWatcherThreadMain(const char* path)
{
	HANDLE hNotify = FindFirstChangeNotificationA(path, true, FILE_NOTIFY_CHANGE_LAST_WRITE);

	while (true)
	{
	}
}

void FileWatcher::Init(const char* path)
{
	s_thread = std::thread(fileWatcherThreadMain, path);
}

void FileWatcher::AddListener(const char* pattern, FileChangedFunc callback)
{

}