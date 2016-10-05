#pragma once
#include "../Types.h"
#include "UtilsLib/Paths.h"
#include "UtilsLib/json/json-forwards.h"

namespace AssetLib
{
	typedef void(*AssetReloadFunc)(const char* assetName);

	class AssetDef
	{
	public:
		static const uint kMaxNameLen = 128;

		AssetDef(const char* folder, const char* ext, uint binVersion);

		void SetReloadHandler(AssetReloadFunc reloadFunc);
		bool HasReloadHandler() const;

		void ExtractAssetName(const char* relativeFilePath, char* outAssetName, uint maxNameLen) const;

		void BuildFilename(const char* assetName, char* outFilename, uint maxFilenameLen) const;

		bool LoadAsset(const char* assetName, char** ppOutFileData, uint* pOutFileSize);
		bool LoadAssetJson(const char* assetName, Json::Value* pJsonRoot);

		void GetFilePattern(char* pOutPattern, uint maxPatternLen);

		const char* GetFolder() const;
		const char* GetExt() const;

		uint GetBinVersion() const;
		uint GetAssetUID() const;

	private:
		static void ReloadCommon(const char* filename, void* pUserData);

		char m_fullPath[FILE_MAX_PATH];
		char m_ext[32];
		uint m_extLen;
		AssetReloadFunc m_reloadFunc;
		char m_folder[64];
		uint m_folderLen;
		uint m_binVersion;
		uint m_assetUID;
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

	inline uint AssetDef::GetBinVersion() const
	{
		return m_binVersion;
	}

	inline uint AssetDef::GetAssetUID() const
	{
		return m_assetUID;
	}
}
