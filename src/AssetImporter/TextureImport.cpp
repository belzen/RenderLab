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
		HeightMap,
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
				else if (_stricmp(typeStr, "h") == 0)
				{
					return TextureType::HeightMap;
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
	else if (_stricmp(ext.c_str(), "tif") == 0)
	{
		hr = DirectX::LoadFromWICMemory(pImageFileData, imageFileSize, 0, &srcMetadata, srcImage);
		assert(hr == S_OK);
	}
	else if (_stricmp(ext.c_str(), "dds") == 0)
	{
		hr = DirectX::LoadFromDDSMemory(pImageFileData, imageFileSize, 0, &srcMetadata, srcImage);
		assert(hr == S_OK);
	}
	else
	{
		Error("Unknown texture file type: %s", ext.c_str());
		delete pImageFileData;
		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	// Determine image settings
	TextureType texType = extractTextureType(srcFilename);
	DXGI_FORMAT format = DXGI_FORMAT_BC1_UNORM;
	DWORD compressFlags = DirectX::TEX_COMPRESS_PARALLEL;
	bool bGenerateMipMaps = true;
	bool bCompress = true;
	switch (texType)
	{
	case TextureType::NormalMap:
		{
			format = DXGI_FORMAT_BC3_UNORM;

			assert(srcMetadata.format == DXGI_FORMAT_R8G8B8A8_UNORM);
			// Prepare texture for DXT5nm compression
			// TODO: Info from Dave Eberly: http://www.gamedev.net/topic/539608-mip-mapping-normal-maps/?whichpage=1%25EF%25BF%25BD
			//		1) Start with 32-bit XYZ normals
			//		2) Generate mip-maps
			//		3) For each 4x4 block:
			//			a) Fit points with a spherical arc
			//			b) Use arc endpoints as DXT endpoints and write DXT 4x4
			uint8* pPixels = srcImage.GetPixels();
			uint count = (uint)(4 * srcMetadata.width * srcMetadata.height);
			for (uint i = 0; i < count; i += 4)
			{
				// Red moved into alpha
				pPixels[i + 3] = pPixels[i + 0];
				// Clear red and blue channels.
				pPixels[i + 0] = 0;
				pPixels[i + 2] = 0;
			}
		}
		break;
	case TextureType::Srgb:
		format = DXGI_FORMAT_BC1_UNORM_SRGB;
		compressFlags |= DirectX::TEX_COMPRESS_SRGB;
		break;
	case TextureType::Linear:
		format = DXGI_FORMAT_BC1_UNORM;
		break;
	case TextureType::HeightMap:
		if (srcImage.GetMetadata().format != DXGI_FORMAT_R16_UNORM)
		{
			Error("Invalid heightmap texture format: %d - %s", srcImage.GetMetadata().format, srcFilename.c_str());
			return false;
		}
		format = DXGI_FORMAT_R16_FLOAT;
		bGenerateMipMaps = false;
		bCompress = false;
		break;
	}

	//////////////////////////////////////////////////////////////////////////
	// Generate mipmaps and compress
	DirectX::ScratchImage mipmapImage;
	DirectX::ScratchImage compressedImage;
	DirectX::ScratchImage* pCurrImage = &srcImage;

	if (bGenerateMipMaps && srcMetadata.width >= 2 && srcMetadata.height >= 2)
	{
		hr = DirectX::GenerateMipMaps(*srcImage.GetImage(0, 0, 0), DirectX::TEX_FILTER_DEFAULT, 0, mipmapImage);
		assert(hr == S_OK);
		pCurrImage = &mipmapImage;
	}

	if (bCompress)
	{
		hr = DirectX::Compress(pCurrImage->GetImages(), pCurrImage->GetImageCount(), pCurrImage->GetMetadata(), format, compressFlags, 1.f, compressedImage);
		assert(hr == S_OK);
		pCurrImage = &compressedImage;
	}

	//////////////////////////////////////////////////////////////////////////
	// Write images
	DirectX::Blob blob;
	hr = DirectX::SaveToDDSMemory(pCurrImage->GetImages(), pCurrImage->GetImageCount(), pCurrImage->GetMetadata(), 0, blob);
	assert(hr == S_OK);

	std::ofstream dstFile(dstFilename, std::ios::binary);
	assert(dstFile.is_open());
	dstFile.write((char*)blob.GetBufferPointer(), blob.GetBufferSize());
	blob.Release();

	delete pImageFileData;
	return true;
}
