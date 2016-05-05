#pragma once
#include "AssetDef.h"

namespace MaterialAsset
{
	static const uint kBinVersion = 1;

	struct BinHeader
	{
		uint unused;
	};

	extern AssetDef Definition;
}