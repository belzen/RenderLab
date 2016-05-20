#include "MaterialBinner.h"
#include "AssetLib/MaterialAsset.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/json/json.h"
#include "UtilsLib/Error.h"

AssetLib::AssetDef& MaterialBinner::GetAssetDef() const
{
	return AssetLib::g_materialDef;
}

std::vector<std::string> MaterialBinner::GetFileTypes() const
{
	std::vector<std::string> types;
	types.push_back("material");
	return types;
}

void MaterialBinner::CalcSourceHash(const std::string& srcFilename, Hashing::SHA1& rOutHash)
{
	char* pSrcFileData;
	uint srcFileSize;
	FileLoader::Load(srcFilename.c_str(), &pSrcFileData, &srcFileSize);

	Hashing::SHA1::Calculate(pSrcFileData, srcFileSize, rOutHash);

	delete pSrcFileData;
}

bool MaterialBinner::BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const
{
	Json::Value jRoot;
	if (!FileLoader::LoadJson(srcFilename.c_str(), jRoot))
	{
		Error("Failed to load material: %s", srcFilename.c_str());
		return false;
	}

	AssetLib::Material binData;
	
	Json::Value jShader = jRoot.get("pixelShader", Json::Value::null);
	strcpy_s(binData.pixelShader, jShader.asCString());

	Json::Value jNeedsLight = jRoot.get("needsLighting", false);
	binData.bNeedsLighting = jNeedsLight.asBool();

	Json::Value jAlphaCutout = jRoot.get("alphaCutout", false);
	binData.bAlphaCutout = jAlphaCutout.asBool();

	Json::Value jTextures = jRoot.get("textures", Json::Value::null);
	binData.texCount = jTextures.size();
	for (uint n = 0; n < binData.texCount; ++n)
	{
		Json::Value jTex = jTextures.get(n, Json::Value::null);
		strcpy_s(binData.textures[n], jTex.asCString());
	}

	dstFile.write((char*)&binData, sizeof(AssetLib::Material));

	return true;
}
