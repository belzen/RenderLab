#pragma once

namespace FileWatcher
{
	typedef int ListenerID;
	typedef void (*FileChangedFunc)(const char* filename, void* pUserData);

	void Init(const char* path);
	ListenerID AddListener(const char* pattern, FileChangedFunc callback, void* pUserData);
	void RemoveListener(const ListenerID listenerId);

	void Cleanup();
}