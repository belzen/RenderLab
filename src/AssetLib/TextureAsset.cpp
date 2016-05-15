#include "TextureAsset.h"
#include <assert.h>

using namespace AssetLib;

AssetLib::AssetDef AssetLib::g_textureDef("textures", "texbin", 1);

Texture* Texture::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == g_textureDef.GetAssetUID());
		pMem += sizeof(BinFileHeader);
	}

	Texture* pTexture = (Texture*)pMem;
	char* pDataMem = pMem + sizeof(Texture);

	pTexture->ddsData.PatchPointer(pDataMem);

	return pTexture;
}
