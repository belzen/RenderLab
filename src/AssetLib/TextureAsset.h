#pragma once
#include "AssetDef.h"
#include "BinFile.h"

namespace AssetLib
{
	struct Texture
	{
		static AssetDef& GetAssetDef();
		static Texture* Load(const char* assetName);

		char* ddsData;
		uint ddsDataSize;

		uint timeLastModified;
		const char* assetName; // Filled in by the AssetLibrary during loading.
	};
}