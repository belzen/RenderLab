#pragma once
#include "AssetDef.h"
#include "MathLib/Vec3.h"
#include "UtilsLib/Color.h"

namespace AssetLib
{
	struct Material
	{
		static AssetDef& GetAssetDef();
		static Material* Load(const CachedString& assetName, Material* pMaterial);

		char vertexShader[AssetLib::AssetDef::kMaxNameLen];
		char pixelShader[AssetLib::AssetDef::kMaxNameLen];

		char** pShaderDefs;
		int shaderDefsCount;

		char** pTextureNames;
		int texCount;

		bool bNeedsLighting;
		bool bAlphaCutout;

		Color color;
		float roughness;
		float metalness;

		uint timeLastModified;
		CachedString assetName;
	};
}