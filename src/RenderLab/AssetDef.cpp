#include "Precompiled.h"

void AssetDef::ReloadCommon(const char* filename, void* pUserData)
{
	AssetDef* pDef = (AssetDef*)pUserData;

	char assetName[AssetDef::kMaxNameLen];
	pDef->ExtractAssetName(filename, assetName, ARRAYSIZE(assetName));

	pDef->m_reloadFunc(assetName);
}

AssetDef::AssetDef(const char* folder, const char* ext)
	: m_reloadFunc(nullptr)
	, m_folder(folder)
	, m_ext(ext)
{
	sprintf_s(m_fullDataPath, "data/%s", folder);
	m_folderLen = (uint)strlen(folder);
	m_extLen = (uint)strlen(ext);
}

void AssetDef::SetReloadHandler(AssetReloadFunc reloadFunc)
{
	if (!m_reloadFunc)
	{
		char filePattern[MAX_PATH];
		GetFilePattern(filePattern, ARRAYSIZE(filePattern));
		FileWatcher::AddListener(filePattern, ReloadCommon, this);
	}

	m_reloadFunc = reloadFunc;
}

void AssetDef::ExtractAssetName(const char* relativeFilePath, char* outAssetName, uint maxNameLen) const
{
	uint pathLen = (uint)strlen(relativeFilePath);
	strncpy_s(outAssetName, maxNameLen, relativeFilePath + m_folderLen + 1, pathLen - m_folderLen - m_extLen - 2);
}

void AssetDef::BuildFilename(const char* assetName, char* outFilename, uint maxFilenameLen) const
{
	sprintf_s(outFilename, maxFilenameLen, "%s/%s.%s", m_fullDataPath, assetName, m_ext);
}

void AssetDef::GetFilePattern(char* pOutPattern, uint maxPatternLen)
{
	sprintf_s(pOutPattern, maxPatternLen, "%s/*.%s", m_folder, m_ext);
}
