#include "ModelAsset.h"

using namespace AssetLib;

AssetLib::AssetDef AssetLib::g_modelDef("geo", "modelbin", 0x6b64afc, 1);

Model* Model::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == g_modelDef.GetAssetUID());
		pMem += sizeof(BinFileHeader);
	}

	Model* pModel = (Model*)pMem;
	char* pDataMem = pMem + sizeof(Model);

	pModel->subobjects.PatchPointer(pDataMem);
	pModel->positions.PatchPointer(pDataMem);
	pModel->texcoords.PatchPointer(pDataMem);
	pModel->normals.PatchPointer(pDataMem);
	pModel->colors.PatchPointer(pDataMem);
	pModel->tangents.PatchPointer(pDataMem);
	pModel->bitangents.PatchPointer(pDataMem);
	pModel->indices.PatchPointer(pDataMem);

	return pModel;
}
