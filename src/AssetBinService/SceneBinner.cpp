#include "SceneBinner.h"
#include "AssetLib/SceneAsset.h"
#include "UtilsLib/FileLoader.h"

int SceneBinner::GetVersion() const
{
	return SceneAsset::kBinVersion;
}

int SceneBinner::GetAssetUID() const
{
	return SceneAsset::kAssetUID;
}

std::string SceneBinner::GetBinExtension() const
{
	return SceneAsset::Definition.GetExt(AssetLoc::Bin);
}

std::vector<std::string> SceneBinner::GetFileTypes() const
{
	std::vector<std::string> types;
	types.push_back(SceneAsset::Definition.GetExt(AssetLoc::Src));
	return types;
}

void SceneBinner::CalcSourceHash(const std::string& srcFilename, SHA1Hash& rOutHash)
{
	char* pSrcFileData;
	uint srcFileSize;
	FileLoader::Load(srcFilename.c_str(), &pSrcFileData, &srcFileSize);

	SHA1Hash::Calculate(pSrcFileData, srcFileSize, rOutHash);

	delete pSrcFileData;
}

bool SceneBinner::BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const
{
	return false;
}