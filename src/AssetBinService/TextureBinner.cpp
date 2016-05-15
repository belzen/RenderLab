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


	struct SourceTexture
	{
		char imagePath[FILE_MAX_PATH];
		DXGI_FORMAT format;
		bool bIsSrgb;
		bool bIsCubemap;
	};

	bool loadSourceTexture(const std::string srcFilename, SourceTexture& rOutTexture)
	{
		Json::Value jRoot;
		if (!FileLoader::LoadJson(srcFilename.c_str(), jRoot))
		{
			assert(false);
			return false;
		}

		Json::Value jTexFilename = jRoot.get("filename", Json::Value::null);
		sprintf_s(rOutTexture.imagePath, "%s/textures/%s", Paths::GetSrcDataDir(), jTexFilename.asCString());

		rOutTexture.bIsSrgb = jRoot.get("srgb", false).asBool();
		rOutTexture.bIsCubemap = jRoot.get("isCubemap", false).asBool();

		Json::Value jFormat = jRoot.get("format", "DXT5");
		DXGI_FORMAT format = DXGI_FORMAT_BC3_TYPELESS;
		if (_stricmp(jFormat.asCString(), "DXT1") == 0)
		{
			rOutTexture.format = DXGI_FORMAT_BC1_UNORM;
		}
		else if (_stricmp(jFormat.asCString(), "DXT5") == 0)
		{
			rOutTexture.format = DXGI_FORMAT_BC3_UNORM;
		}
		else if (_stricmp(jFormat.asCString(), "DXT5nm") == 0)
		{
			rOutTexture.format = DXGI_FORMAT_BC3_UNORM;
		}

		return true;
	}
}

AssetLib::AssetDef& TextureBinner::GetAssetDef() const
{
	return AssetLib::g_textureDef;
}

std::vector<std::string> TextureBinner::GetFileTypes() const
{
	std::vector<std::string> types;
	types.push_back("tex");
	return types;
}

void TextureBinner::CalcSourceHash(const std::string& srcFilename, SHA1Hash& rOutHash)
{
	SourceTexture tex;
	if (!loadSourceTexture(srcFilename, tex))
		return;

	char* pImageFileData;
	uint imageFileSize;
	if (!FileLoader::Load(tex.imagePath, &pImageFileData, &imageFileSize))
		return;

	SHA1HashState* pHash = SHA1Hash::Begin((char*)&tex, sizeof(SourceTexture));
	SHA1Hash::Update(pHash, pImageFileData, imageFileSize);
	SHA1Hash::Finish(pHash, rOutHash);

	delete pImageFileData;
}

bool TextureBinner::BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const
{
	SourceTexture tex;
	if (!loadSourceTexture(srcFilename, tex))
		return false;

	// Load the texture image file.
	char* pImageFileData;
	uint imageFileSize;
	if (!FileLoader::Load(tex.imagePath, &pImageFileData, &imageFileSize))
	{
		assert(false);
		return false;
	}

	AssetLib::Texture binData;
	binData.bIsCubemap = tex.bIsCubemap;
	binData.bIsSrgb = tex.bIsSrgb;
	binData.ddsData.offset = 0;

	dstFile.write((char*)&binData, sizeof(AssetLib::Texture));

	DirectX::ScratchImage srcImage;
	DirectX::ScratchImage compressedImage;

	bool res = true;
	std::string ext = getFileExtension(tex.imagePath);
	if (_stricmp(ext.c_str(), "tga") == 0)
	{
		HRESULT hr;

		DirectX::TexMetadata srcMetadata;
		hr = DirectX::LoadFromTGAMemory(pImageFileData, imageFileSize, &srcMetadata, srcImage);
		assert(hr == S_OK);

		DirectX::ScratchImage mipmaps;
		hr = DirectX::GenerateMipMaps(*srcImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, mipmaps);
		assert(hr == S_OK);

		DirectX::ScratchImage compressedImage;
		DWORD compressFlags = DirectX::TEX_COMPRESS_DEFAULT;
		if (tex.bIsSrgb)
			compressFlags |= DirectX::TEX_COMPRESS_SRGB;
		hr = DirectX::Compress(mipmaps.GetImages(), mipmaps.GetImageCount(), mipmaps.GetMetadata(), tex.format, compressFlags, 1.f, compressedImage);
		assert(hr == S_OK);

		DirectX::Blob blob;
		hr = DirectX::SaveToDDSMemory(compressedImage.GetImages(), compressedImage.GetImageCount(), compressedImage.GetMetadata(), 0, blob);
		assert(hr == S_OK);
		
		dstFile.write((char*)blob.GetBufferPointer(), blob.GetBufferSize());
		blob.Release();
	}
	else if (_stricmp(ext.c_str(), "dds") == 0)
	{
		dstFile.write(pImageFileData, imageFileSize);
	}
	else
	{
		Error("Unknown texture file type: %s", ext.c_str());
		res = false;
	}

	delete pImageFileData;
	return true;
}