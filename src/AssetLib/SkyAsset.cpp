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

Sky* Sky::Load(const char* assetName)
{
	Json::Value jRoot;
	if (!GetAssetDef().LoadAssetJson(assetName, &jRoot))
	{
		Error("Failed to load sky asset: %s", assetName);
		return nullptr;
	}

	Sky* pSky = new Sky();

	Json::Value jModel = jRoot.get("model", Json::Value::null);
	strcpy_s(pSky->model, jModel.asCString());

	Json::Value jMaterialName = jRoot.get("material", Json::Value::null);
	strcpy_s(pSky->material, jMaterialName.asCString());

	pSky->sun.angleXAxis = jRoot.get("sunAngleXAxis", 0.f).asFloat();
	pSky->sun.angleZAxis = jRoot.get("sunAngleZAxis", 0.f).asFloat();
	pSky->sun.intensity = jRoot.get("sunIntensity", 0.f).asFloat();
	pSky->sun.color = jsonReadVec3(jRoot.get("sunColor", Json::Value::null));

	pSky->shadows.pssmLambda = jRoot.get("pssmLambda", 0.f).asFloat();

	// Volumetric fog
	{
		Json::Value jFog = jRoot.get("volumetricFog", Json::Value::null);
		pSky->volumetricFog.scatteringCoeff = jsonReadVec3(jFog.get("scatteringCoeff", Json::Value::null));
		pSky->volumetricFog.absorptionCoeff = jsonReadVec3(jFog.get("absorptionCoeff", Json::Value::null));
		pSky->volumetricFog.phaseG = jFog.get("phaseG", 0.f).asFloat();
		pSky->volumetricFog.farDepth = jFog.get("farDepth", 0.f).asFloat();
	}

	return pSky;
}

