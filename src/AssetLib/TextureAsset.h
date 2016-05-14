#pragma once
#include "AssetDef.h"
#include "BinFile.h"

namespace TextureAsset
{
	static const uint kBinVersion = 1;
	static const uint kAssetUID = 0xee1d1f87;

	extern AssetDef Definition;

	struct BinData
	{
		static BinData* FromMem(char* pMem);

		bool bIsCubemap;
		bool bIsSrgb;
		BinDataPtr<char> ddsData;
	};
}