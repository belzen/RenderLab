#include "PostProcessEffectsAsset.h"
#include <assert.h>

using namespace AssetLib;

AssetLib::AssetDef AssetLib::g_postProcessEffectsDef("postproc", "ppfxbin", 0xac919b7e, 1);

PostProcessEffects* PostProcessEffects::FromMem(char* pMem)
{
	if (((uint*)pMem)[0] == BinFileHeader::kUID)
	{
		BinFileHeader* pHeader = (BinFileHeader*)pMem;
		assert(pHeader->assetUID == g_postProcessEffectsDef.GetAssetUID());
		pMem += sizeof(BinFileHeader);
	}

	PostProcessEffects* pEffects = (PostProcessEffects*)pMem;
	return pEffects;
}
