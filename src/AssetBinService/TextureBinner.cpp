#include "TextureBinner.h"
#include "AssetLib/TextureAsset.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/json/json.h"
#include "UtilsLib/Error.h"

#include <assert.h>

#include <DirectXTex/include/DirectXTex.h>
#include <DirectXTex/include/Dds.h>

#pragma comment (lib, "DirectXTex.lib")

namespace
{
	std::string getFileExtension(const std::string& filename)
	{
		uint pos = (uint)filename.find_last_of('.');
		return filename.substr(pos + 1);
	}
}

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
	// Read in settings from ".texture" file, including the name of the actual image file.
	Json::Value jRoot;
	if (!FileLoader::LoadJson(srcFilename.c_str(), jRoot))
	{
		assert(false);
		return 0;
	}

	char filepath[FILE_MAX_PATH];
	Json::Value jTexFilename = jRoot.get("filename", Json::Value::null);
	sprintf_s(filepath, "%s/textures/%s", Paths::GetSrcDataDir(), jTexFilename.asCString());

	bool bIsSrgb = jRoot.get("srgb", false).asBool();
	bool bIsCubemap = jRoot.get("isCubemap", false).asBool();

	Json::Value jFormat = jRoot.get("format", "DXT5");
	DXGI_FORMAT format = DXGI_FORMAT_BC3_TYPELESS;
	if (_stricmp(jFormat.asCString(), "DXT1") == 0)
	{
		format = DXGI_FORMAT_BC1_UNORM;
	}
	else if (_stricmp(jFormat.asCString(), "DXT5") == 0)
	{
		format = DXGI_FORMAT_BC3_UNORM;
	}
	else if (_stricmp(jFormat.asCString(), "DXT5nm") == 0)
	{
		format = DXGI_FORMAT_BC3_UNORM;
	}

	// Load the texture image file.
	char* pFileData;
	uint fileSize;
	if (!FileLoader::Load(filepath, &pFileData, &fileSize))
	{
		assert(false);
		return false;
	}

	TextureAsset::BinData binData;
	binData.bIsCubemap = bIsCubemap;
	binData.bIsSrgb = bIsSrgb;
	binData.ddsData.offset = 0;

	dstFile.write((char*)&binData, sizeof(TextureAsset::BinData));


	DirectX::ScratchImage srcImage;
	DirectX::ScratchImage compressedImage;

	bool res = true;
	std::string ext = getFileExtension(filepath);
	if (_stricmp(ext.c_str(), "tga") == 0)
	{
		HRESULT hr;

		DirectX::TexMetadata srcMetadata;
		hr = DirectX::LoadFromTGAMemory(pFileData, fileSize, &srcMetadata, srcImage);
		assert(hr == S_OK);

		DirectX::ScratchImage mipmaps;
		hr = DirectX::GenerateMipMaps(*srcImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, mipmaps);
		assert(hr == S_OK);

		DirectX::ScratchImage compressedImage;
		DWORD compressFlags = DirectX::TEX_COMPRESS_DEFAULT;
		if (bIsSrgb)
			compressFlags |= DirectX::TEX_COMPRESS_SRGB;
		hr = DirectX::Compress(mipmaps.GetImages(), mipmaps.GetImageCount(), mipmaps.GetMetadata(), format, compressFlags, 1.f, compressedImage);
		assert(hr == S_OK);

		DirectX::Blob blob;
		hr = DirectX::SaveToDDSMemory(compressedImage.GetImages(), compressedImage.GetImageCount(), compressedImage.GetMetadata(), 0, blob);
		assert(hr == S_OK);
		
		dstFile.write((char*)blob.GetBufferPointer(), blob.GetBufferSize());
		blob.Release();
	}
	else if (_stricmp(ext.c_str(), "dds") == 0)
	{
		dstFile.write(pFileData, fileSize);
	}
	else
	{
		Error("Unknown texture file type: %s", ext.c_str());
		res = false;
	}

	delete pFileData;
	return true;
}