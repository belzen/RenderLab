#include "SkyAsset.h"
#include "BinFile.h"
#include <assert.h>

using namespace AssetLib;

AssetLib::AssetDef AssetLib::Sky::s_definition("skies", "skybin", 1);

Sky* Sky::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == s_definition.GetAssetUID());
		pMem += sizeof(BinFileHeader);
	}

	Sky* pSky = (Sky*)pMem;
	return pSky;
}
