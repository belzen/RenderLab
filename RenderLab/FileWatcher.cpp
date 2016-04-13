#include "Precompiled.h"
#include "FileWatcher.h"
#include <windows.h>
#include <assert.h>
#include <thread>
#include <fileapi.h>

namespace
{
	struct
	{
		std::thread thread;
		HANDLE hDirectory;
		OVERLAPPED overlapped;
		BYTE notifyBuffer[16 * 1024];
		char path[MAX_PATH];
		bool bTerminate;

	} s_fileWatcher;

	void reissueFileListener();

	void CALLBACK notificationCompletion(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
		BYTE* pCur = s_fileWatcher.notifyBuffer;
		while (true)
		{
			FILE_NOTIFY_INFORMATION* pInfo = (FILE_NOTIFY_INFORMATION*)pCur;

			// todo: Compare changed file against search patterns, send to callbacks.
			// todo: File modifications seem to come through here twice...

			if (pInfo->NextEntryOffset == 0)
				break;
			pCur += pInfo->NextEntryOffset;
		}

		// The listen operation has to be reissued after every completion routine.
		reissueFileListener();
	}

	void reissueFileListener()
	{
		const DWORD dwNotificationFlags = FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION | FILE_NOTIFY_CHANGE_FILE_NAME;
		DWORD dwBytes = 0;

		BOOL bSuccess = ReadDirectoryChangesW(
			s_fileWatcher.hDirectory, s_fileWatcher.notifyBuffer, sizeof(s_fileWatcher.notifyBuffer),
			true, dwNotificationFlags, &dwBytes, &s_fileWatcher.overlapped, &notificationCompletion);

		// Failure only acceptable if the file watcher is being terminated.
		assert(bSuccess || s_fileWatcher.bTerminate);
	}

	void fileWatcherThreadMain()
	{
		s_fileWatcher.hDirectory = ::CreateFileA(
			s_fileWatcher.path,
			FILE_LIST_DIRECTORY, 
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL);

		assert(s_fileWatcher.hDirectory != INVALID_HANDLE_VALUE);

		reissueFileListener();

		while (!s_fileWatcher.bTerminate)
		{
			// Sleep in alertable state so that the completion routine from reissueFileListener() can execute.
			SleepEx(INFINITE, true);
		}
	}
}


void FileWatcher::Init(const char* dataDir)
{
	GetCurrentDirectoryA(MAX_PATH, s_fileWatcher.path);
	strcat_s(s_fileWatcher.path, MAX_PATH, "/");
	strcat_s(s_fileWatcher.path, MAX_PATH, dataDir);

	s_fileWatcher.thread = std::thread(fileWatcherThreadMain);
}

void FileWatcher::AddListener(const char* pattern, FileChangedFunc callback)
{
	// todo
}

void FileWatcher::Cleanup()
{
	s_fileWatcher.bTerminate = true;
	CancelIo(s_fileWatcher.hDirectory);
	CloseHandle(s_fileWatcher.hDirectory);
	s_fileWatcher.thread.join();
}
