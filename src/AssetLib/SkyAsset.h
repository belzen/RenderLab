#pragma once
#include "AssetDef.h"
#include "MathLib/Vec3.h"

namespace AssetLib
{
	struct Sky
	{
		static AssetDef& GetAssetDef();
		static Sky* Load(const char* assetName);

		char model[AssetLib::AssetDef::kMaxNameLen];
		char material[AssetLib::AssetDef::kMaxNameLen];

		struct
		{
			Vec3 color;
			float angleXAxis; // Degrees
			float angleZAxis; // Degrees
			float intensity;
		} sun;

		struct  
		{
			float pssmLambda;
		} shadows;

		uint timeLastModified;
		const char* assetName; // Filled in by the AssetLibrary during loading.
	};
}