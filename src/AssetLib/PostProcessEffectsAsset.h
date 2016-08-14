#pragma once
#include "AssetDef.h"
#include "BinFile.h"

namespace AssetLib
{
	struct PostProcessEffects
	{
		static AssetDef s_definition;
		static PostProcessEffects* FromMem(char* pMem);

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
		} bloom;

		uint timeLastModified;
		const char* assetName; // Filled in by the AssetLibrary during loading.
	};
}
