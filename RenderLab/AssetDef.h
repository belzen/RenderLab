#pragma once

typedef void (*AssetReloadFunc)(const char* assetName);

class AssetDef
{
public:
	static const uint kMaxNameLen = 128;

	AssetDef(const char* folder, const char* ext);

	void SetReloadHandler(AssetReloadFunc reloadFunc);
	bool HasReloadHandler() const;

	void ExtractAssetName(const char* relativeFilePath, char* outAssetName, uint maxNameLen) const;

	void BuildFilename(const char* assetName, char* outFilename, uint maxFilenameLen) const;

	void GetFilePattern(char* pOutPattern, uint maxPatternLen);

	const char* GetFolder() const;
	const char* GetExt() const;

private:
	static void ReloadCommon(const char* filename, void* pUserData);

	char m_fullDataPath[MAX_PATH];

	AssetReloadFunc m_reloadFunc;

	const char* m_folder;
	uint m_folderLen;

	const char* m_ext;
	uint m_extLen;
};

inline bool AssetDef::HasReloadHandler() const
{
	return m_reloadFunc != nullptr;
}

inline const char* AssetDef::GetFolder() const
{
	return m_folder;
}

inline const char* AssetDef::GetExt() const
{
	return m_ext;
}
