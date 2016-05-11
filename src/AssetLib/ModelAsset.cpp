#include "ModelAsset.h"

AssetDef ModelAsset::Definition("geo", "model");

ModelAsset::BinData* ModelAsset::BinData::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == ModelAsset::kAssetUID);
		pMem += sizeof(BinFileHeader);
	}

	ModelAsset::BinData* pBin = (ModelAsset::BinData*)pMem;
	char* pDataMem = pMem + sizeof(ModelAsset::BinData);

	pBin->subobjects.PatchPointer(pDataMem);
	pBin->positions.PatchPointer(pDataMem);
	pBin->texcoords.PatchPointer(pDataMem);
	pBin->normals.PatchPointer(pDataMem);
	pBin->colors.PatchPointer(pDataMem);
	pBin->tangents.PatchPointer(pDataMem);
	pBin->bitangents.PatchPointer(pDataMem);
	pBin->indices.PatchPointer(pDataMem);

	return pBin;
}