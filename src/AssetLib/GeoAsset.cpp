#include "GeoAsset.h"

AssetDef GeoAsset::Definition("geo", "geo");

GeoAsset::BinFile* GeoAsset::BinFile::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID )
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == GeoAsset::kAssetUID);
		pMem += sizeof(BinFileHeader);
	}

	GeoAsset::BinFile* pBin = (GeoAsset::BinFile*)pMem;
	char* pDataMem = pMem + sizeof(GeoAsset::BinFile);

	pBin->positions.PatchPointer(pDataMem);
	pBin->texcoords.PatchPointer(pDataMem);
	pBin->normals.PatchPointer(pDataMem);
	pBin->colors.PatchPointer(pDataMem);
	pBin->tangents.PatchPointer(pDataMem);
	pBin->bitangents.PatchPointer(pDataMem);
	pBin->tris.PatchPointer(pDataMem);

	return pBin;
}