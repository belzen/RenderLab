#include "Precompiled.h"
#include "RdrMaterial.h"
#include "RdrShaderSystem.h"
#include "RdrResourceSystem.h"
#include "AssetLib/MaterialAsset.h"
#include "UtilsLib/Hash.h"

namespace
{
	typedef std::map<Hashing::StringHash, RdrMaterial*> RdrMaterialMap;
	typedef FreeList<RdrMaterial, 1024> RdrMaterialList;

	RdrMaterialList s_materials;
	RdrMaterialMap s_materialCache;

	void loadMaterial(const char* materialName, RdrMaterial* pOutMaterial)
	{
		char* pFileData;
		uint fileSize;
		if (!AssetLib::g_materialDef.LoadAsset(materialName, &pFileData, &fileSize))
		{
			assert(false);
			return;
		}

		AssetLib::Material* pMaterial = AssetLib::Material::FromMem(pFileData);

		// Create default pixel shader
		const char* defines[2] = { 0 };
		uint numDefines = 0;
		if (pMaterial->bAlphaCutout)
		{
			defines[0] = "ALPHA_CUTOUT";
			++numDefines;
		}

		pOutMaterial->hPixelShaders[(int)RdrShaderMode::Normal] = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);

		// Alpha cutout requires a depth-only pixel shader.
		if (pMaterial->bAlphaCutout)
		{
			defines[numDefines] = "DEPTH_ONLY";
			++numDefines;
			pOutMaterial->hPixelShaders[(int)RdrShaderMode::DepthOnly] = RdrShaderSystem::CreatePixelShaderFromFile(pMaterial->pixelShader, defines, numDefines);
		}

		pOutMaterial->bNeedsLighting = pMaterial->bNeedsLighting;
		pOutMaterial->bAlphaCutout = pMaterial->bAlphaCutout;
		pOutMaterial->texCount = pMaterial->texCount;

		int numTextures = pOutMaterial->texCount;
		for (int n = 0; n < numTextures; ++n)
		{
			pOutMaterial->hTextures[n] = RdrResourceSystem::CreateTextureFromFile(pMaterial->textures[n], nullptr);
			pOutMaterial->samplers[n] = RdrSamplerState(RdrComparisonFunc::Never, RdrTexCoordMode::Wrap, false);
		}

		delete pFileData;
	}
}

const RdrMaterial* RdrMaterial::LoadFromFile(const char* materialName)
{
	if (!AssetLib::g_materialDef.HasReloadHandler())
	{
		AssetLib::g_materialDef.SetReloadHandler(RdrMaterial::ReloadMaterial);
	}

	// Check if the material's already been loaded
	Hashing::StringHash nameHash = Hashing::HashString(materialName);
	RdrMaterialMap::iterator iter = s_materialCache.find(nameHash);
	if (iter != s_materialCache.end())
		return iter->second;

	// Material not cached, create it.
	RdrMaterial* pMaterial = s_materials.allocSafe();

	loadMaterial(materialName, pMaterial);

	s_materialCache.insert(std::make_pair(nameHash, pMaterial));

	return pMaterial;
}

void RdrMaterial::ReloadMaterial(const char* materialName)
{
	Hashing::StringHash nameHash = Hashing::HashString(materialName);
	RdrMaterialMap::iterator iter = s_materialCache.find(nameHash);
	if (iter != s_materialCache.end())
	{
		loadMaterial(materialName, iter->second);
	}
}
