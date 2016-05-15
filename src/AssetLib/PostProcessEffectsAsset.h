#pragma once
#include "AssetDef.h"
#include "BinFile.h"

namespace AssetLib
{
	extern AssetDef g_postProcessEffectsDef;

	struct PostProcessEffects
	{
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
	};
}
