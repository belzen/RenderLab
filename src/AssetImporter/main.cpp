#define NOMINMAX
#include <Windows.h>
#include <assert.h>
#include "ModelImport.h"
#include "TextureImport.h"
#include "UtilsLib/Paths.h"
#include "UtilsLib/Util.h"
#include "AssetLib/AssetDef.h"
#include "AssetLib/BinFile.h"
#include "AssetLib/ModelAsset.h"
#include "AssetLib/TextureAsset.h"

namespace
{
	void createDirectoryTreeForFile(const std::string& filename)
	{
		int endPos = -1;
		while ((endPos = (int)filename.find_first_of("/\\", endPos + 1)) != std::string::npos)
		{
			std::string dir = filename.substr(0, endPos);
			CreateDirectoryA(dir.c_str(), nullptr);
		}
	}

	std::string getOutputFilename(const char* filename, const AssetLib::AssetDef& rAssetDef)
	{
		char assetName[256];
		Paths::GetFilenameNoExtension(filename, assetName, ARRAY_SIZE(assetName));

		std::string outFilename = Paths::GetDataDir();
		outFilename += "/";
		outFilename += rAssetDef.GetFolder();
		outFilename += assetName;
		outFilename += ".";
		outFilename += rAssetDef.GetExt();

		createDirectoryTreeForFile(outFilename);

		return outFilename;
	}
}

int main(int argc, char** argv)
{
	if (argc <= 1)
	{
		printf("Usage: <filename>\n");
		return -1;
	}

	// DirectXTex initialization
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(hr == S_OK);

	const char* filename = argv[1];
	const char* ext = Paths::GetExtension(filename);

	if (_stricmp(ext, "obj") == 0)
	{
		std::string outFilename = getOutputFilename(filename, AssetLib::Model::GetAssetDef());
		ModelImport::ImportObj(filename, outFilename);
	}
	else if (_stricmp(ext, "fbx") == 0)
	{
		std::string outFilename = getOutputFilename(filename, AssetLib::Model::GetAssetDef());
		ModelImport::ImportFbx(filename, outFilename);
	}
	else if (_stricmp(ext, "tga") == 0 || _stricmp(ext, "tif") == 0 || _stricmp(ext, "dds") == 0)
	{
		std::string outFilename = getOutputFilename(filename, AssetLib::Texture::GetAssetDef());
		TextureImport::Import(filename, outFilename);
	}
	else
	{
		printf("Unknown file type: %s\n", filename);
		return -2;
	}


	return 0;
}