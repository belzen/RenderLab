#pragma once
#include "../Types.h"
#include "UtilsLib/Paths.h"

typedef void (*AssetReloadFunc)(const char* assetName);

enum class AssetLoc
{
	Src,
	Bin,

	Count
};

class AssetDef
{
public:
	static const uint kMaxNameLen = 128;

	AssetDef(const char* folder, const char* srcExt);

	void SetReloadHandler(AssetLoc loc, AssetReloadFunc reloadFunc);
	bool HasReloadHandler(AssetLoc loc) const;

	void ExtractAssetName(const char* relativeFilePath, char* outAssetName, uint maxNameLen) const;

	void BuildFilename(AssetLoc loc, const char* assetName, char* outFilename, uint maxFilenameLen) const;

	void GetFilePattern(AssetLoc loc, char* pOutPattern, uint maxPatternLen);

	const char* GetFolder() const;
	const char* GetExt(AssetLoc loc) const;

private:
	static void ReloadSrcCommon(const char* filename, void* pUserData);
	static void ReloadBinCommon(const char* filename, void* pUserData);

	struct AssetLocData
	{
		char fullPath[FILE_MAX_PATH];
		char ext[32];
		uint extLen;
		AssetReloadFunc reloadFunc;
	};

	AssetLocData m_locData[(int)AssetLoc::Count];

	char m_folder[64];
	uint m_folderLen;
};

inline bool AssetDef::HasReloadHandler(AssetLoc loc) const
{
	return m_locData[(int)loc].reloadFunc != nullptr;
}

inline const char* AssetDef::GetFolder() const
{
	return m_folder;
}

inline const char* AssetDef::GetExt(AssetLoc loc) const
{
	return m_locData[(int)loc].ext;
}
