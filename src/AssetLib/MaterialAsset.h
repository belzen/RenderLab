#pragma once
#include "AssetDef.h"

namespace MaterialAsset
{
	static const uint kBinVersion = 1;
	static const uint kAssetUID = 0x1f432ed9;

	extern AssetDef Definition;

	struct BinHeader
	{
		uint unused;
	};

	extern AssetDef Definition;
}