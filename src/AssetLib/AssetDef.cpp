#include "AssetDef.h"
#include "UtilsLib/Util.h"
#include "UtilsLib/FileWatcher.h"
#include "UtilsLib/Paths.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Hash.h"

using namespace AssetLib;

namespace 
{
	void cstrRemoveExtension(char* str)
	{
		for (int i = (int)strlen(str) - 1; i > 0; --i)
		{
			if (str[i] == '.')
			{
				str[i] = 0;
				return;
			}
		}
	}
}

void AssetDef::ReloadCommon(const char* filename, void* pUserData)
{
	AssetDef* pDef = (AssetDef*)pUserData;

	char assetName[AssetDef::kMaxNameLen];
	pDef->ExtractAssetName(filename, assetName, ARRAY_SIZE(assetName));

	pDef->m_reloadFunc(assetName);
}

AssetDef::AssetDef(const char* folder, const char* binExt, uint binVersion)
	: m_binVersion(binVersion)
{
	strcpy_s(m_folder, folder);
	m_folderLen = (uint)strlen(m_folder);

	sprintf_s(m_fullPath, "%s/%s", Paths::GetBinDataDir(), m_folder);
	strcpy_s(m_ext, binExt);
	m_extLen = (uint)strlen(m_ext);

	m_assetUID = Hashing::HashString(binExt);
}

void AssetDef::SetReloadHandler(AssetReloadFunc reloadFunc)
{
	if (!m_reloadFunc)
	{
		char filePattern[FILE_MAX_PATH];
		GetFilePattern(filePattern, ARRAY_SIZE(filePattern));
		FileWatcher::AddListener(filePattern, ReloadCommon, this);
	}

	m_reloadFunc = reloadFunc;
}

void AssetDef::ExtractAssetName(const char* relativeFilePath, char* outAssetName, uint maxNameLen) const
{
	uint pathLen = (uint)strlen(relativeFilePath);
	strncpy_s(outAssetName, maxNameLen, relativeFilePath + m_folderLen + 1, pathLen - m_folderLen - 1);
	cstrRemoveExtension(outAssetName);
}

void AssetDef::BuildFilename(const char* assetName, char* outFilename, uint maxFilenameLen) const
{
	sprintf_s(outFilename, maxFilenameLen, "%s/%s.%s", m_fullPath, assetName, m_ext);
}

bool AssetDef::LoadAsset(const char* assetName, char** ppOutFileData, uint* pOutFileSize)
{
	char filePath[FILE_MAX_PATH];
	BuildFilename(assetName, filePath, ARRAY_SIZE(filePath));

	return FileLoader::Load(filePath, ppOutFileData, pOutFileSize);
}

void AssetDef::GetFilePattern(char* pOutPattern, uint maxPatternLen)
{
	sprintf_s(pOutPattern, maxPatternLen, "%s/*.%s", m_folder, m_ext);
}
