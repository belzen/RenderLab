#include "TextureAsset.h"
#include "UtilsLib\JsonUtils.h"
#include "UtilsLib\FileLoader.h"
#include "UtilsLib\Error.h"

using namespace AssetLib;

AssetDef& Texture::GetAssetDef()
{
	static AssetDef s_assetDef("textures", "dds", 1);
	return s_assetDef;
}

Texture* Texture::Load(const char* assetName, Texture* pTexture)
{
	char* pFileData = nullptr;
	uint fileSize = 0;

	if (!GetAssetDef().LoadAsset(assetName, &pFileData, &fileSize))
	{
		Error("Failed to load texture asset: %s", assetName);
		return pTexture;
	}

	if (!pTexture)
	{
		pTexture = new Texture();
	}

	pTexture->ddsData = pFileData;
	pTexture->ddsDataSize = fileSize;
	return pTexture;
}
