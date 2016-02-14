#pragma once

namespace FileWatcher
{
	typedef void (*FileChangedFunc)(const char* filename);

	void Init(const char* path);
	void AddListener(const char* pattern, FileChangedFunc callback);
}