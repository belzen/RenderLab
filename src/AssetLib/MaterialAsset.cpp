#include "MaterialAsset.h"
#include "BinFile.h"
#include "UtilsLib/JsonUtils.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Error.h"
#include <assert.h>

using namespace AssetLib;

AssetDef& Material::GetAssetDef()
{
	static AssetLib::AssetDef s_assetDef("materials", "material", 2);
	return s_assetDef;
}

Material* Material::Load(const CachedString& assetName, Material* pMaterial)
{
	Json::Value jRoot;
	if (!GetAssetDef().LoadAssetJson(assetName.getString(), &jRoot))
	{
		Error("Failed to load material asset: %s", assetName.getString());
		return pMaterial;
	}

	if (!pMaterial)
	{
		pMaterial = new Material();
	}

	Json::Value jVertexShader = jRoot.get("vertexShader", Json::Value::null);
	strcpy_s(pMaterial->vertexShader, jVertexShader.asCString());

	Json::Value jPixelShader = jRoot.get("pixelShader", Json::Value::null);
	strcpy_s(pMaterial->pixelShader, jPixelShader.asCString());

	pMaterial->bNeedsLighting = jRoot.get("needsLighting", false).asBool();
	pMaterial->bAlphaCutout = jRoot.get("alphaCutout", false).asBool();
	pMaterial->roughness = jRoot.get("roughness", 1.0f).asFloat();
	pMaterial->metalness = jRoot.get("metalness", 0.0f).asFloat();

	Json::Value jColor = jRoot.get("color", Json::Value::null);
	pMaterial->color = jColor.isArray() ? jsonReadColor(jColor) : Color::kWhite;

	Json::Value jShaderDefs = jRoot.get("shaderDefs", Json::Value::null);
	pMaterial->shaderDefsCount = jShaderDefs.size();
	pMaterial->pShaderDefs = new char*[pMaterial->shaderDefsCount];
	for (int n = 0; n < pMaterial->shaderDefsCount; ++n)
	{
		Json::Value jDef = jShaderDefs.get(n, Json::Value::null);
		pMaterial->pShaderDefs[n] = _strdup(jDef.asCString());
	}

	Json::Value jTextures = jRoot.get("textures", Json::Value::null);
	pMaterial->texCount = jTextures.size();
	pMaterial->pTextureNames = new char*[pMaterial->texCount];
	for (int n = 0; n < pMaterial->texCount; ++n)
	{
		Json::Value jTex = jTextures.get(n, Json::Value::null);
		pMaterial->pTextureNames[n] = _strdup(jTex.asCString());
	}

	pMaterial->assetName = assetName;
	return pMaterial;
}
