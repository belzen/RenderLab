#include "ModelAsset.h"
#include "UtilsLib/Error.h"
#include "UtilsLib/Util.h"

using namespace AssetLib;

AssetDef& Model::GetAssetDef()
{
	static AssetLib::AssetDef s_assetDef("geo", "model", 1);
	return s_assetDef;
}

Model* Model::Load(const char* assetName, Model* pModel)
{
	char* pFileData = nullptr;
	uint fileSize = 0;

	SAFE_DELETE(pModel);

	if (!GetAssetDef().LoadAsset(assetName, &pFileData, &fileSize))
	{
		Error("Failed to load model asset: %s", assetName);
		return pModel;
	}

	if (((uint*)pFileData)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pFileData;
		assert(pHeader->assetUID == GetAssetDef().GetAssetUID());
		pFileData += sizeof(BinFileHeader);
	}

	pModel = (Model*)pFileData;
	char* pDataMem = pFileData + sizeof(Model);

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
