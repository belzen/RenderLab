#include "SkyBinner.h"
#include "AssetLib/SkyAsset.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/json/json.h"
#include "UtilsLib/Error.h"

AssetLib::AssetDef& SkyBinner::GetAssetDef() const
{
	return AssetLib::g_skyDef;
}

std::vector<std::string> SkyBinner::GetFileTypes() const
{
	std::vector<std::string> types;
	types.push_back("sky");
	return types;
}

void SkyBinner::CalcSourceHash(const std::string& srcFilename, SHA1Hash& rOutHash)
{
	char* pSrcFileData;
	uint srcFileSize;
	FileLoader::Load(srcFilename.c_str(), &pSrcFileData, &srcFileSize);

	SHA1Hash::Calculate(pSrcFileData, srcFileSize, rOutHash);

	delete pSrcFileData;
}

bool SkyBinner::BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const
{
	Json::Value jRoot;
	if (!FileLoader::LoadJson(srcFilename.c_str(), jRoot))
	{
		Error("Failed to load sky: %s", srcFilename.c_str());
		return false;
	}

	AssetLib::Sky binData;

	Json::Value jModel = jRoot.get("model", Json::Value::null);
	strcpy_s(binData.model, jModel.asCString());

	Json::Value jMaterialName = jRoot.get("material", Json::Value::null);
	strcpy_s(binData.material, jMaterialName.asCString());

	dstFile.write((char*)&binData, sizeof(AssetLib::Sky));

	return true;
}
