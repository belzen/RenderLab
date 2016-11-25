#pragma once
#include "AssetDef.h"

namespace AssetLib
{
	struct Material
	{
		static AssetDef& GetAssetDef();
		static Material* Load(const CachedString& assetName, Material* pMaterial);

		char pixelShader[AssetLib::AssetDef::kMaxNameLen];
		char textures[16][AssetLib::AssetDef::kMaxNameLen];
		uint texCount;
		bool bNeedsLighting;
		bool bAlphaCutout;

		float roughness;
		float metalness;

		uint timeLastModified;
		CachedString assetName;
	};
}