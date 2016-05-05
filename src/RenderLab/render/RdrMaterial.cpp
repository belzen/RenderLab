#include "Precompiled.h"
#include "RdrMaterial.h"
#include "RdrShaderSystem.h"
#include "RdrResourceSystem.h"
#include "AssetLib/MaterialAsset.h"

namespace
{
	typedef std::map<std::string, RdrMaterial*, StringInvariantCompare> RdrMaterialMap;
	typedef FreeList<RdrMaterial, 1024> RdrMaterialList;

	RdrMaterialList s_materials;
	RdrMaterialMap s_materialCache;

	void loadMaterial(const char* materialName, RdrMaterial* pOutMaterial)
	{
		char fullFilename[FILE_MAX_PATH];
		MaterialAsset::Definition.BuildFilename(AssetLoc::Src, materialName, fullFilename, ARRAY_SIZE(fullFilename));

		Json::Value root;
		if (!FileLoader::LoadJson(fullFilename, root))
		{
			Error("Failed to load material: %s", fullFilename);
			return;
		}

		Json::Value jShader = root.get("pixelShader", Json::Value::null);
		pOutMaterial->hPixelShaders[(int)RdrShaderMode::Normal] = RdrShaderSystem::CreatePixelShaderFromFile(jShader.asCString());

		Json::Value jNeedsLight = root.get("needsLighting", Json::Value::null);
		pOutMaterial->bNeedsLighting = jNeedsLight.asBool();

		Json::Value jTextures = root.get("textures", Json::Value::null);
		int numTextures = pOutMaterial->texCount = jTextures.size();
		for (int n = 0; n < numTextures; ++n)
		{
			Json::Value jTex = jTextures.get(n, Json::Value::null);

			pOutMaterial->hTextures[n] = RdrResourceSystem::CreateTextureFromFile(jTex.asCString(), nullptr);
			pOutMaterial->samplers[n] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
		}
	}
}

const RdrMaterial* RdrMaterial::LoadFromFile(const char* materialName)
{
	if (!MaterialAsset::Definition.HasReloadHandler(AssetLoc::Src))
	{
		MaterialAsset::Definition.SetReloadHandler(AssetLoc::Src, RdrMaterial::ReloadMaterial);
	}

	// Check if the material's already been loaded
	RdrMaterialMap::iterator iter = s_materialCache.find(materialName);
	if (iter != s_materialCache.end())
		return iter->second;

	// Material not cached, create it.
	RdrMaterial* pMaterial = s_materials.allocSafe();

	loadMaterial(materialName, pMaterial);

	s_materialCache.insert(std::make_pair(materialName, pMaterial));

	return pMaterial;
}

void RdrMaterial::ReloadMaterial(const char* materialName)
{
	RdrMaterialMap::iterator iter = s_materialCache.find(materialName);
	if (iter != s_materialCache.end())
	{
		loadMaterial(materialName, iter->second);
	}
}
