#pragma once
#include "AssetDef.h"
#include "MathLib/Vec3.h"

namespace AssetLib
{
	struct VolumetricFogSettings
	{
		Vec3 scatteringCoeff;
		Vec3 absorptionCoeff;
		float phaseG;
		float farDepth;
		bool enabled;
	};

	struct Sky
	{
		static AssetDef& GetAssetDef();
		static Sky* Load(const char* assetName, Sky* pSky);

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

		VolumetricFogSettings volumetricFog;

		uint timeLastModified;
		const char* assetName; // Filled in by the AssetLibrary during loading.
	};
}