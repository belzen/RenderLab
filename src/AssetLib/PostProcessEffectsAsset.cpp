#include "PostProcessEffectsAsset.h"
#include <assert.h>

using namespace AssetLib;

AssetLib::AssetDef AssetLib::PostProcessEffects::s_definition("postproc", "ppfxbin", 1);

PostProcessEffects* PostProcessEffects::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == s_definition.GetAssetUID());
		pMem += sizeof(BinFileHeader);
	}

	PostProcessEffects* pEffects = (PostProcessEffects*)pMem;
	return pEffects;
}
