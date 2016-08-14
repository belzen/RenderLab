#include "PostProcessEffectsBinner.h"
#include "AssetLib/PostProcessEffectsAsset.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/json/json.h"
#include "UtilsLib/Error.h"

AssetLib::AssetDef& PostProcessEffectsBinner::GetAssetDef() const
{
	return AssetLib::PostProcessEffects::s_definition;
}

std::vector<std::string> PostProcessEffectsBinner::GetFileTypes() const
{
	std::vector<std::string> types;
	types.push_back("ppfx");
	return types;
}

void PostProcessEffectsBinner::CalcSourceHash(const std::string& srcFilename, Hashing::SHA1& rOutHash)
{
	char* pSrcFileData;
	uint srcFileSize;
	FileLoader::Load(srcFilename.c_str(), &pSrcFileData, &srcFileSize);

	Hashing::SHA1::Calculate(pSrcFileData, srcFileSize, rOutHash);

	delete pSrcFileData;
}

bool PostProcessEffectsBinner::BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const
{
	Json::Value jRoot;
	if (!FileLoader::LoadJson(srcFilename.c_str(), jRoot))
	{
		Error("Failed to load post-process effects: %s", srcFilename.c_str());
		return false;
	}

	AssetLib::PostProcessEffects binData;

	// Bloom
	{
		Json::Value jBloom = jRoot.get("bloom", Json::Value::null);
		binData.bloom.threshold = jBloom.get("threshold", 1.f).asFloat();
	}

	// Eye adaptation
	{
		Json::Value jEyeAdaptation = jRoot.get("eyeAdaptation", Json::Value::null);
		binData.eyeAdaptation.white = jEyeAdaptation.get("white", 12.5f).asFloat();
		binData.eyeAdaptation.middleGrey = jEyeAdaptation.get("middleGrey", 0.5f).asFloat();
		binData.eyeAdaptation.minExposure = jEyeAdaptation.get("minExposure", -16.f).asFloat();
		binData.eyeAdaptation.maxExposure = jEyeAdaptation.get("maxExposure", 16.f).asFloat();
	}

	dstFile.write((char*)&binData, sizeof(AssetLib::PostProcessEffects));

	return true;
}
