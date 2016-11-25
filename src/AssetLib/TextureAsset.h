#pragma once
#include "AssetDef.h"
#include "BinFile.h"

namespace AssetLib
{
	struct Texture
	{
		static AssetDef& GetAssetDef();
		static Texture* Load(const CachedString& assetName, Texture* pTexture);

		char* ddsData;
		uint ddsDataSize;

		uint timeLastModified;
		CachedString assetName;
	};
}