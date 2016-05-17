#include "FileWatcher.h"
#include <windows.h>
#include <thread>
#include <fileapi.h>
#include <regex>
#include <set>
#include <assert.h>
#include "ThreadMutex.h"
#include "../Types.h"
#include "Paths.h"
#include "Timer.h"

namespace
{
	struct FileListener
	{
		FileWatcher::ListenerID id;
		std::regex patternRegex;
		FileWatcher::FileChangedFunc changedFunc;
		void* pUserData;
	};

	typedef std::set<std::string> StringSet;

	struct
	{
		std::thread listenerThread;
		std::thread callbackThread;

		HANDLE hDirectory;
		OVERLAPPED overlapped;
		char notifyBuffer[16 * 1024];
		char path[FILE_MAX_PATH];
		uint pathLen;
		volatile bool bTerminate;

		HANDLE hCallbackEvent;

		Timer::Handle hChangedTimer;
		ThreadMutex changedMutex;
		StringSet changedFiles;

		ThreadMutex listenerMutex;
		std::vector<FileListener> listeners;
		FileWatcher::ListenerID nextListenerId;
	} s_fileWatcher;

	void reissueFileListener();

	void CALLBACK notificationCompletion(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
	{
		char* pCur = s_fileWatcher.notifyBuffer;

		AutoScopedLock listenerLock(s_fileWatcher.listenerMutex);
		AutoScopedLock changedLock(s_fileWatcher.changedMutex);

		bool bFoundListeners = false;

		while (true)
		{
			FILE_NOTIFY_INFORMATION* pInfo = (FILE_NOTIFY_INFORMATION*)pCur;

			char relativePath[FILE_MAX_PATH];
			size_t numConverted;
			wcstombs_s(&numConverted, relativePath, pInfo->FileName, pInfo->FileNameLength / sizeof(wchar_t));

			for (uint i = 0; i < (uint)s_fileWatcher.listeners.size(); ++i)
			{
				FileListener& rListener = s_fileWatcher.listeners[i];
				if (regex_match(relativePath, rListener.patternRegex))
				{
					// Add file to the change list.  Processing is done on the callback thread as a file modification
					// often results in multiple events, so the processing is delayed to receive all events.
					s_fileWatcher.changedFiles.insert(relativePath);
					bFoundListeners = true;

					// For this phase, we just need to know if there's any listener for this file, so stop searching.
					break;
				}
			}

			if (pInfo->NextEntryOffset == 0)
				break;
			pCur += pInfo->NextEntryOffset;
		}

		if (bFoundListeners)
		{
			Timer::Reset(s_fileWatcher.hChangedTimer);
			SetEvent(s_fileWatcher.hCallbackEvent);
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

	void fileWatcherListenerThread()
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

	void fileWatcherCallbackThread()
	{
		while (!s_fileWatcher.bTerminate)
		{
			WaitForMultipleObjects(1, &s_fileWatcher.hCallbackEvent, true, INFINITE);
			if (s_fileWatcher.bTerminate)
				break;

			// Wait a short amount of time since the last change to catch subsequent file changes.  
			// The ReadDirectoryChangesW often sends multiple events for a file modification.
			DWORD msTimeDiff;
			s_fileWatcher.changedMutex.Lock();
			while ((msTimeDiff = (DWORD)(Timer::GetElapsedSeconds(s_fileWatcher.hChangedTimer) * 1000)) < 500)
			{
				s_fileWatcher.changedMutex.Unlock();
				Sleep(msTimeDiff);
				s_fileWatcher.changedMutex.Lock();
			} while (msTimeDiff < 500);

			for (StringSet::iterator changedIter = s_fileWatcher.changedFiles.begin(); changedIter != s_fileWatcher.changedFiles.end(); ++changedIter)
			{
				const std::string& rRelativePath = *changedIter;
				for (uint i = 0; i < (uint)s_fileWatcher.listeners.size(); ++i)
				{
					FileListener& rListener = s_fileWatcher.listeners[i];
					if (regex_match(rRelativePath.c_str(), rListener.patternRegex))
					{
						rListener.changedFunc(rRelativePath.c_str(), rListener.pUserData);
					}
				}
			}

			s_fileWatcher.changedFiles.clear();
			s_fileWatcher.changedMutex.Unlock();
		}
	}

	void strPrefixChar(std::string& str, char c, const char* prefix)
	{
		size_t pos = str.length();
		while ((pos = str.rfind(c, pos)) != std::string::npos)
		{
			str.insert(pos, prefix);
			if (pos == 0)
				break;
			--pos;
		}
	}
}

void FileWatcher::Init(const char* path)
{
	strcpy_s(s_fileWatcher.path, path);
	s_fileWatcher.pathLen = (uint)strlen(s_fileWatcher.path);
	s_fileWatcher.nextListenerId = 1;
	s_fileWatcher.hCallbackEvent = CreateEventA(nullptr, false, false, nullptr);
	s_fileWatcher.hChangedTimer = Timer::Create();

	s_fileWatcher.listenerThread = std::thread(fileWatcherListenerThread);
	s_fileWatcher.callbackThread = std::thread(fileWatcherCallbackThread);
}

FileWatcher::ListenerID FileWatcher::AddListener(const char* pattern, FileChangedFunc callback, void* pUserData)
{
	FileListener listener;

	// Convert file system pattern to regex.
	std::string patternRegex = pattern;
	std::replace(patternRegex.begin(), patternRegex.end(), '/', '\\');
	strPrefixChar(patternRegex, '\\', "\\");
	strPrefixChar(patternRegex, '.', "\\");
	strPrefixChar(patternRegex, '*', ".");

	listener.patternRegex = patternRegex;
	listener.changedFunc = callback;
	listener.pUserData = pUserData;
	listener.id = s_fileWatcher.nextListenerId++;

	AutoScopedLock lock(s_fileWatcher.listenerMutex);
	s_fileWatcher.listeners.push_back(listener);

	return listener.id;
}

void FileWatcher::RemoveListener(const ListenerID listenerId)
{
	AutoScopedLock lock(s_fileWatcher.listenerMutex);

	uint numListeners = (uint)s_fileWatcher.listeners.size();
	for (uint i = 0; i < numListeners; ++i)
	{
		FileListener& rListener = s_fileWatcher.listeners[i];
		if (rListener.id == listenerId)
		{
			s_fileWatcher.listeners.erase(s_fileWatcher.listeners.begin() + i);
			break;
		}
	}
}

void FileWatcher::Cleanup()
{
	s_fileWatcher.bTerminate = true;

	SetEvent(s_fileWatcher.hCallbackEvent);
	s_fileWatcher.callbackThread.join();

	CancelIo(s_fileWatcher.hDirectory);
	CloseHandle(s_fileWatcher.hDirectory);
	CloseHandle(s_fileWatcher.hCallbackEvent);
	s_fileWatcher.listenerThread.join();
}
