#pragma once
#include "AssetDef.h"

namespace SceneAsset
{
	static const uint kBinVersion = 1;

	struct BinHeader
	{
		uint unused;
	};

	extern AssetDef Definition;
}