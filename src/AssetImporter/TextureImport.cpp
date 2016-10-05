#include "TextureImport.h"
#include "AssetLib/TextureAsset.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Error.h"
#include "UtilsLib/Util.h"
#include <assert.h>

#include "DirectXTex/include/DirectXTex.h"
#include "DirectXTex/include/Dds.h"

namespace
{
	enum class TextureType
	{
		Srgb,
		Linear,
		NormalMap,
	};

	TextureType extractTextureType(const std::string& srcFilename)
	{
		char assetName[256];
		Paths::GetFilenameNoExtension(srcFilename.c_str(), assetName, ARRAY_SIZE(assetName));

		int nameLen = (int)strlen(assetName);
		for (int i = nameLen - 1; i > 0; --i)
		{
			if (assetName[i] == '_')
			{
				const char* typeStr = assetName + i + 1;
				if (_stricmp(typeStr, "ddn") == 0)
				{
					return TextureType::NormalMap;
				}
				else if (_stricmp(typeStr, "l") == 0)
				{
					return TextureType::Linear;
				}
				else if (_stricmp(typeStr, "spec") == 0)
				{
					return TextureType::Linear;
				}
			}
		}

		return TextureType::Srgb;
	}
}

bool TextureImport::Import(const std::string& srcFilename, std::string& dstFilename)
{
	const AssetLib::AssetDef& rAssetDef = AssetLib::Texture::GetAssetDef();

	//////////////////////////////////////////////////////////////////////////
	// Load the texture image file.
	char* pImageFileData;
	uint imageFileSize;
	if (!FileLoader::Load(srcFilename.c_str(), &pImageFileData, &imageFileSize))
	{
		assert(false);
		return false;
	}

	// Create image by type
	HRESULT hr;
	std::string ext = Paths::GetExtension(srcFilename.c_str());
	DirectX::ScratchImage srcImage;
	DirectX::TexMetadata srcMetadata;
	if (_stricmp(ext.c_str(), "tga") == 0)
	{
		hr = DirectX::LoadFromTGAMemory(pImageFileData, imageFileSize, &srcMetadata, srcImage);
		assert(hr == S_OK);
	}
	else
	{
		Error("Unknown texture file type: %s", ext.c_str());
		delete pImageFileData;
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Generate mip maps
	DirectX::ScratchImage mipmaps;
	hr = DirectX::GenerateMipMaps(*srcImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, mipmaps);
	assert(hr == S_OK);

	//////////////////////////////////////////////////////////////////////////
	// Compress image
	TextureType texType = extractTextureType(srcFilename);
	DXGI_FORMAT format = DXGI_FORMAT_BC1_UNORM;
	DWORD compressFlags = DirectX::TEX_COMPRESS_DEFAULT;
	switch (texType)
	{
	case TextureType::NormalMap:
		// TODO: DXT5nm
		format = DXGI_FORMAT_BC3_UNORM;
		break;
	case TextureType::Srgb:
		format = DXGI_FORMAT_BC1_UNORM_SRGB;
		compressFlags |= DirectX::TEX_COMPRESS_SRGB;
		break;
	case TextureType::Linear:
		format = DXGI_FORMAT_BC1_UNORM;
		break;
	}

	DirectX::ScratchImage compressedImage;
	hr = DirectX::Compress(mipmaps.GetImages(), mipmaps.GetImageCount(), mipmaps.GetMetadata(), format, compressFlags, 1.f, compressedImage);
	assert(hr == S_OK);

	//////////////////////////////////////////////////////////////////////////
	// Write images
	DirectX::Blob blob;
	hr = DirectX::SaveToDDSMemory(compressedImage.GetImages(), compressedImage.GetImageCount(), compressedImage.GetMetadata(), 0, blob);
	assert(hr == S_OK);

	std::ofstream dstFile(dstFilename, std::ios::binary);
	assert(dstFile.is_open());
	dstFile.write((char*)blob.GetBufferPointer(), blob.GetBufferSize());
	blob.Release();

	delete pImageFileData;
	return true;
}
