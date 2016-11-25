#include "SkyAsset.h"
#include "BinFile.h"
#include "UtilsLib/JsonUtils.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Error.h"

using namespace AssetLib;

AssetDef& Sky::GetAssetDef()
{
	static AssetDef s_assetDef("skies", "sky", 1);
	return s_assetDef;
}

Sky* Sky::Load(const CachedString& assetName, Sky* pSky)
{
	Json::Value jRoot;
	if (!GetAssetDef().LoadAssetJson(assetName.getString(), &jRoot))
	{
		Error("Failed to load sky asset: %s", assetName.getString());
		return pSky;
	}

	if (!pSky)
	{
		pSky = new Sky();
	}

	jsonReadCachedString(jRoot.get("model", Json::Value::null), &pSky->modelName);
	jsonReadCachedString(jRoot.get("material", Json::Value::null), &pSky->materialName);

	pSky->sun.angleXAxis = jRoot.get("sunAngleXAxis", 0.f).asFloat();
	pSky->sun.angleZAxis = jRoot.get("sunAngleZAxis", 0.f).asFloat();
	pSky->sun.intensity = jRoot.get("sunIntensity", 0.f).asFloat();
	pSky->sun.color = jsonReadVec3(jRoot.get("sunColor", Json::Value::null));

	pSky->shadows.pssmLambda = jRoot.get("pssmLambda", 0.f).asFloat();

	// Volumetric fog
	{
		Json::Value jFog = jRoot.get("volumetricFog", Json::Value::null);
		pSky->volumetricFog.enabled = jFog.get("enabled", true).asBool();
		pSky->volumetricFog.scatteringCoeff = jsonReadVec3(jFog.get("scatteringCoeff", Json::Value::null));
		pSky->volumetricFog.absorptionCoeff = jsonReadVec3(jFog.get("absorptionCoeff", Json::Value::null));
		pSky->volumetricFog.phaseG = jFog.get("phaseG", 0.f).asFloat();
		pSky->volumetricFog.farDepth = jFog.get("farDepth", 0.f).asFloat();
	}

	pSky->assetName = assetName;
	return pSky;
}

