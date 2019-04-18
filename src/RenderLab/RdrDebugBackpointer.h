#pragma once

#define CREATE_BACKPOINTER(pSrc) RdrDebugBackpointer((pSrc), __FILE__, __LINE__)
#define CREATE_NULL_BACKPOINTER RdrDebugBackpointer(__FILE__, __LINE__)

struct RdrDebugBackpointer
{
	RdrDebugBackpointer() = default;
	RdrDebugBackpointer(const char* filename, uint lineNum) : pSource(nullptr), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const void* pSrc, const char* filename, uint lineNum) : pSource(pSrc), strFilename(filename), nLineNum(lineNum) {}

	~RdrDebugBackpointer() = default;

	const void* pSource;
	const char* strFilename;
	uint nLineNum;
};