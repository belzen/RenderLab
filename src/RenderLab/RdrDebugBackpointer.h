#pragma once

class RdrLighting;
class Decal;
class ModelComponent;
class ModelData;
class RdrSky;
class Terrain;
class Sprite;
class Ocean;
class RdrAction;
struct RdrMaterial;
class RdrPostProcess;
class Renderer;

#define CREATE_BACKPOINTER(pSrc) RdrDebugBackpointer(pSrc, __FILE__, __LINE__)
#define CREATE_NULL_BACKPOINTER RdrDebugBackpointer(__FILE__, __LINE__)

struct RdrDebugBackpointer
{
	// donotcheckin - smoother type handling
	RdrDebugBackpointer() = default;
	RdrDebugBackpointer(const char* filename, uint lineNum) : pSource(nullptr), nDataType(0), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const RdrLighting* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(0), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const RdrSky* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(1), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const Decal* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(2), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const ModelComponent* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(3), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const ModelData* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(4), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const Terrain* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(5), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const Sprite* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(6), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const Ocean* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(7), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const RdrAction* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(8), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const RdrMaterial* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(9), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const Renderer* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(10), strFilename(filename), nLineNum(lineNum) {}
	RdrDebugBackpointer(const RdrPostProcess* pSrc, const char* filename, uint lineNum) : pSource(pSrc), nDataType(11), strFilename(filename), nLineNum(lineNum) {}

	~RdrDebugBackpointer() = default;

	const void* pSource;
	const char* strFilename;
	uint nLineNum;
	uint nDataType;
};