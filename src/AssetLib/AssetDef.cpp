#include "AssetDef.h"
#include "UtilsLib/Util.h"
#include "UtilsLib/FileWatcher.h"
#include "UtilsLib/Paths.h"

namespace 
{
	void strRemoveExtension(char* str)
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

void AssetDef::ReloadSrcCommon(const char* filename, void* pUserData)
{
	AssetDef* pDef = (AssetDef*)pUserData;

	char assetName[AssetDef::kMaxNameLen];
	pDef->ExtractAssetName(filename, assetName, ARRAY_SIZE(assetName));

	pDef->m_locData[(int)AssetLoc::Src].reloadFunc(assetName);
}

void AssetDef::ReloadBinCommon(const char* filename, void* pUserData)
{
	AssetDef* pDef = (AssetDef*)pUserData;

	char assetName[AssetDef::kMaxNameLen];
	pDef->ExtractAssetName(filename, assetName, ARRAY_SIZE(assetName));

	pDef->m_locData[(int)AssetLoc::Bin].reloadFunc(assetName);
}

AssetDef::AssetDef(const char* folder, const char* srcExt)
{
	strcpy_s(m_folder, folder);
	m_folderLen = (uint)strlen(m_folder);

	AssetLocData& rSrcLoc = m_locData[(int)AssetLoc::Src];
	sprintf_s(rSrcLoc.fullPath, "%s/%s", Paths::GetSrcDataDir(), m_folder);
	strcpy_s(rSrcLoc.ext, srcExt);
	rSrcLoc.extLen = (uint)strlen(rSrcLoc.ext);

	AssetLocData& rBinLoc = m_locData[(int)AssetLoc::Bin];
	sprintf_s(rBinLoc.fullPath, "%s/%s", Paths::GetBinDataDir(), m_folder);
	sprintf_s(rBinLoc.ext, "%sbin", srcExt);
	rBinLoc.extLen = (uint)strlen(rBinLoc.ext);
}

void AssetDef::SetReloadHandler(AssetLoc loc, AssetReloadFunc reloadFunc)
{
	AssetLocData& rLocData = m_locData[(int)loc];
	if (!rLocData.reloadFunc)
	{
		char filePattern[FILE_MAX_PATH];
		GetFilePattern(loc, filePattern, ARRAY_SIZE(filePattern));
		FileWatcher::AddListener(filePattern, (loc == AssetLoc::Src) ? ReloadSrcCommon : ReloadBinCommon, this);
	}

	rLocData.reloadFunc = reloadFunc;
}

void AssetDef::ExtractAssetName(const char* relativeFilePath, char* outAssetName, uint maxNameLen) const
{
	uint pathLen = (uint)strlen(relativeFilePath);
	strncpy_s(outAssetName, maxNameLen, relativeFilePath + m_folderLen + 1, pathLen - m_folderLen - 1);
	strRemoveExtension(outAssetName);
}

void AssetDef::BuildFilename(AssetLoc loc, const char* assetName, char* outFilename, uint maxFilenameLen) const
{
	const AssetLocData& rLocData = m_locData[(int)loc];
	sprintf_s(outFilename, maxFilenameLen, "%s/%s.%s", rLocData.fullPath, assetName, rLocData.ext);
}

void AssetDef::GetFilePattern(AssetLoc loc, char* pOutPattern, uint maxPatternLen)
{
	const AssetLocData& rLocData = m_locData[(int)loc];
	sprintf_s(pOutPattern, maxPatternLen, "%s/*.%s", m_folder, rLocData.ext);
}
