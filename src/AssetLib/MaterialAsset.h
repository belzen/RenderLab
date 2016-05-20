#pragma once
#include "AssetDef.h"

namespace AssetLib
{
	extern AssetDef g_materialDef;

	struct Material
	{
		static Material* FromMem(char* pMem);

		char pixelShader[AssetLib::AssetDef::kMaxNameLen];
		char textures[16][AssetLib::AssetDef::kMaxNameLen];
		uint texCount;
		bool bNeedsLighting;
		bool bAlphaCutout;
	};
}