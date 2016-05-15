#include "SceneAsset.h"
#include <assert.h>

using namespace AssetLib;

AssetLib::AssetDef AssetLib::g_sceneDef("scenes", "scenebin", 0x10c728ca, 1);

Scene* Scene::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == g_sceneDef.GetAssetUID());
		pMem += sizeof(BinFileHeader);
	}

	Scene* pScene = (Scene*)pMem;
	char* pDataMem = pMem + sizeof(Scene);

	pScene->lights.PatchPointer(pDataMem);
	pScene->objects.PatchPointer(pDataMem);

	return pScene;
}
