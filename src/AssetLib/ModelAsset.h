#pragma once
#include "AssetDef.h"

namespace ModelAsset
{
	static const uint kBinVersion = 1;

	struct BinHeader
	{
		uint unused;
	};

	extern AssetDef Definition;
}