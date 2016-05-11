#pragma once
#include "AssetDef.h"

namespace TextureAsset
{
	static const uint kBinVersion = 1;
	static const uint kAssetUID = 0xee1d1f87;

	extern AssetDef Definition;

	struct BinHeader
	{
		uint unused;
	};
}