#pragma once
#include "AssetDef.h"

namespace AssetLib
{
	extern AssetDef g_skyDef;

	struct Sky
	{
		static Sky* FromMem(char* pMem);

		char model[AssetLib::AssetDef::kMaxNameLen];
		char material[AssetLib::AssetDef::kMaxNameLen];
	};
}