#pragma once
#include "AssetDef.h"
#include "MathLib/Vec3.h"
#include "UtilsLib/StringCache.h"

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
		static Sky* Load(const CachedString& assetName, Sky* pSky);

		CachedString modelName;
		CachedString materialName;

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
		CachedString assetName;
	};
}