#pragma once
#include "AssetDef.h"
#include "BinFile.h"

namespace AssetLib
{
	extern AssetDef g_textureDef;

	struct Texture
	{
		static Texture* FromMem(char* pMem);

		bool bIsCubemap;
		bool bIsSrgb;
		BinDataPtr<char> ddsData;
	};
}