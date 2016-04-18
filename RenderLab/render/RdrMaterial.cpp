#include "Precompiled.h"
#include "RdrMaterial.h"
#include "FileLoader.h"
#include "json/json.h"
#include "RdrShaderSystem.h"
#include "RdrResourceSystem.h"

namespace
{
	typedef std::map<std::string, RdrMaterial*, StringInvariantCompare> MaterialMap;
	typedef FreeList<RdrMaterial, 1024> RdrMaterialList;

	RdrMaterialList s_materials;
	MaterialMap s_materialCache;
}

const RdrMaterial* RdrMaterial::LoadFromFile(const char* materialName)
{
	// Check if the material's already been loaded
	MaterialMap::iterator iter = s_materialCache.find(materialName);
	if (iter != s_materialCache.end())
		return iter->second;

	// Material not cached, create it.
	RdrMaterial* pMaterial = s_materials.allocSafe();

	char fullFilename[MAX_PATH];
	sprintf_s(fullFilename, "data/materials/%s.material", materialName);

	Json::Value root;
	FileLoader::LoadJson(fullFilename, root);

	Json::Value jShader = root.get("pixelShader", Json::Value::null);
	pMaterial->hPixelShaders[(int)RdrShaderMode::Normal] = RdrShaderSystem::CreatePixelShaderFromFile(jShader.asCString());

	Json::Value jNeedsLight = root.get("needsLighting", Json::Value::null);
	pMaterial->bNeedsLighting = jNeedsLight.asBool();

	Json::Value jTextures = root.get("textures", Json::Value::null);
	int numTextures = pMaterial->texCount = jTextures.size();
	for (int n = 0; n < numTextures; ++n)
	{
		Json::Value jTex = jTextures.get(n, Json::Value::null);

		pMaterial->hTextures[n] = RdrResourceSystem::CreateTextureFromFile(jTex.asCString(), nullptr);
		pMaterial->samplers[n] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
	}


	s_materialCache.insert(std::make_pair(materialName, pMaterial));

	return pMaterial;
}
