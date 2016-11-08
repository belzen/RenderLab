#include "MaterialAsset.h"
#include "BinFile.h"
#include "UtilsLib/json/json.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Error.h"
#include <assert.h>

using namespace AssetLib;

AssetDef& Material::GetAssetDef()
{
	static AssetLib::AssetDef s_assetDef("materials", "material", 2);
	return s_assetDef;
}

Material* Material::Load(const char* assetName, Material* pMaterial)
{
	Json::Value jRoot;
	if (!GetAssetDef().LoadAssetJson(assetName, &jRoot))
	{
		Error("Failed to load material asset: %s", assetName);
		return pMaterial;
	}

	if (!pMaterial)
	{
		pMaterial = new Material();
	}

	Json::Value jShader = jRoot.get("pixelShader", Json::Value::null);
	strcpy_s(pMaterial->pixelShader, jShader.asCString());

	pMaterial->bNeedsLighting = jRoot.get("needsLighting", false).asBool();
	pMaterial->bAlphaCutout = jRoot.get("alphaCutout", false).asBool();
	pMaterial->roughness = jRoot.get("roughness", 1.0f).asFloat();
	pMaterial->metalness = jRoot.get("metalness", 0.0f).asFloat();

	Json::Value jTextures = jRoot.get("textures", Json::Value::null);
	pMaterial->texCount = jTextures.size();
	for (uint n = 0; n < pMaterial->texCount; ++n)
	{
		Json::Value jTex = jTextures.get(n, Json::Value::null);
		strcpy_s(pMaterial->textures[n], jTex.asCString());
	}

	return pMaterial;
}
