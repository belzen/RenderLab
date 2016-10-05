#pragma once
#include "AssetDef.h"

namespace AssetLib
{
	struct Material
	{
		static AssetDef& GetAssetDef();
		static Material* Load(const char* assetName);

		char pixelShader[AssetLib::AssetDef::kMaxNameLen];
		char textures[16][AssetLib::AssetDef::kMaxNameLen];
		uint texCount;
		bool bNeedsLighting;
		bool bAlphaCutout;

		uint timeLastModified;
		const char* assetName; // Filled in by the AssetLibrary during loading.
	};
}