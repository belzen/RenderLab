#pragma once
#include "AssetDef.h"

namespace SkyAsset
{
	static const uint kBinVersion = 1;
	static const uint kAssetUID = 0xb2239f27;

	extern AssetDef Definition;

	struct BinHeader
	{
		uint unused;
	};
}