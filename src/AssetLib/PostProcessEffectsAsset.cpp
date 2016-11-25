#include "PostProcessEffectsAsset.h"
#include "UtilsLib/json/json.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Error.h"

using namespace AssetLib;

AssetDef& PostProcessEffects::GetAssetDef()
{
	static AssetLib::AssetDef s_assetDef("postproc", "ppfx", 1);
	return s_assetDef;
}

PostProcessEffects* PostProcessEffects::Load(const CachedString& assetName, PostProcessEffects* pEffects)
{
	Json::Value jRoot;
	if (!GetAssetDef().LoadAssetJson(assetName.getString(), &jRoot))
	{
		Error("Failed to load post-processing effects asset: %s", assetName.getString());
		return pEffects;
	}

	if (!pEffects)
	{
		pEffects = new PostProcessEffects();
	}

	// Bloom
	Json::Value jBloom = jRoot.get("bloom", Json::Value::null);
	pEffects->bloom.enabled = jBloom.get("enabled", true).asBool();
	pEffects->bloom.threshold = jBloom.get("threshold", 1.f).asFloat();

	// Eye adaptation
	Json::Value jEyeAdaptation = jRoot.get("eyeAdaptation", Json::Value::null);
	pEffects->eyeAdaptation.white = jEyeAdaptation.get("white", 12.5f).asFloat();
	pEffects->eyeAdaptation.middleGrey = jEyeAdaptation.get("middleGrey", 0.5f).asFloat();
	pEffects->eyeAdaptation.minExposure = jEyeAdaptation.get("minExposure", -16.f).asFloat();
	pEffects->eyeAdaptation.maxExposure = jEyeAdaptation.get("maxExposure", 16.f).asFloat();

	pEffects->assetName = assetName;
	return pEffects;
}
