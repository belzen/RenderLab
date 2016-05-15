#include "MaterialAsset.h"
#include "BinFile.h"
#include <assert.h>

using namespace AssetLib;

AssetLib::AssetDef AssetLib::g_materialDef("materials", "matbin", 0x1f432ed9, 1);

Material* Material::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == g_materialDef.GetAssetUID());
		pMem += sizeof(BinFileHeader);
	}

	Material* pMaterial = (Material*)pMem;
	return pMaterial;
}
