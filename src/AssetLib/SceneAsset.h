#pragma once
#include "AssetDef.h"

namespace SceneAsset
{
	static const uint kBinVersion = 1;
	static const uint kAssetUID = 0x10c728ca;

	extern AssetDef Definition;

	struct BinHeader
	{
		uint unused;
	};
}