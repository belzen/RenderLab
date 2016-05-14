#include "TextureAsset.h"
#include <assert.h>

AssetDef TextureAsset::Definition("textures", "tex");

TextureAsset::BinData* TextureAsset::BinData::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == TextureAsset::kAssetUID);
		pMem += sizeof(BinFileHeader);
	}

	TextureAsset::BinData* pBin = (TextureAsset::BinData*)pMem;
	char* pDataMem = pMem + sizeof(TextureAsset::BinData);

	pBin->ddsData.PatchPointer(pDataMem);

	return pBin;
}
