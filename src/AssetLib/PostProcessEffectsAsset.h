#pragma once
#include "AssetDef.h"
#include "BinFile.h"

namespace AssetLib
{
	struct PostProcessEffects
	{
		static AssetDef& GetAssetDef();
		static PostProcessEffects* Load(const CachedString& assetName, PostProcessEffects* pEffects);

		struct
		{
			float white;
			float middleGrey;
			float minExposure;
			float maxExposure;
		} eyeAdaptation;

		struct
		{
			float threshold;
			bool enabled;
		} bloom;

		uint timeLastModified;
		CachedString assetName;
	};
}
