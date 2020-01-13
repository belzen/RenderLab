#include "ModelAsset.h"
#include "UtilsLib/Error.h"
#include "UtilsLib/Util.h"

using namespace AssetLib;

AssetDef& Model::GetAssetDef()
{
	static AssetLib::AssetDef s_assetDef("geo", "model", 3);
	return s_assetDef;
}

Model* Model::Load(const CachedString& assetName, Model* pModel)
{
	char* pFileData = nullptr;
	uint fileSize = 0;

	SAFE_DELETE(pModel);

	if (!GetAssetDef().LoadAsset(assetName.getString(), &pFileData, &fileSize))
	{
		Error("Failed to load model asset: %s", assetName.getString());
		return pModel;
	}

	BinFileHeader* pHeader = (BinFileHeader*)pFileData;
	if (pHeader->binUID != BinFileHeader::kUID)
	{
		Error("Invalid model bin ID: %s (Got %d, Expected %)", assetName.getString(), pHeader->binUID, BinFileHeader::kUID);
		return nullptr;
	}

	Assert(pHeader->assetUID == GetAssetDef().GetAssetUID());

	if (pHeader->version == GetAssetDef().GetBinVersion())
	{
		pModel = (Model*)(pFileData + sizeof(BinFileHeader));
		char* pDataMem = pFileData + sizeof(BinFileHeader) + sizeof(Model);

		pModel->subobjects.PatchPointer(pDataMem);
		pModel->inputElements.PatchPointer(pDataMem);
		pModel->positions.PatchPointer(pDataMem);
		pModel->vertexBuffer.PatchPointer(pDataMem);
		pModel->indexBuffer.PatchPointer(pDataMem);
		pModel->assetName = assetName.getString();
	}
	else
	{
		Error("Model version mismatch: %s (Got %d, Expected %)", assetName.getString(), pHeader->version, GetAssetDef().GetBinVersion());
		return pModel;
	}
	
	return pModel;
}
