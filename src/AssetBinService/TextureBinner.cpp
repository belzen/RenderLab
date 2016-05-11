#include "TextureBinner.h"
#include "AssetLib/TextureAsset.h"
#include "UtilsLib/FileLoader.h"

int TextureBinner::GetVersion() const
{
	return TextureAsset::kBinVersion;
}

int TextureBinner::GetAssetUID() const
{
	return TextureAsset::kAssetUID;
}

std::string TextureBinner::GetBinExtension() const
{
	return TextureAsset::Definition.GetExt(AssetLoc::Bin);
}

std::vector<std::string> TextureBinner::GetFileTypes() const
{
	std::vector<std::string> types;
	types.push_back(TextureAsset::Definition.GetExt(AssetLoc::Src));
	return types;
}

void TextureBinner::CalcSourceHash(const std::string& srcFilename, SHA1Hash& rOutHash)
{
	char* pSrcFileData;
	uint srcFileSize;
	FileLoader::Load(srcFilename.c_str(), &pSrcFileData, &srcFileSize);

	SHA1Hash::Calculate(pSrcFileData, srcFileSize, rOutHash);

	delete pSrcFileData;
}

bool TextureBinner::BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const
{
	return false;
}