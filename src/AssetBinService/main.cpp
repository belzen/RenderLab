#define NOMINMAX
#include <Windows.h>
#include <fstream>
#include <map>
#include <direct.h>
#include <assert.h>
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/FileWatcher.h"
#include "UtilsLib/Util.h"
#include "UtilsLib/Paths.h"
#include "AssetLib/BinFile.h"
#include "ModelBinner.h"
#include "TextureBinner.h"
#include "SceneBinner.h"
#include "SkyBinner.h"
#include "MaterialBinner.h"
#include "PostProcessEffectsBinner.h"

typedef std::map<std::string, IAssetBinner*, StringInvariantCompare> AssetBinnerMap;
AssetBinnerMap g_assetBinners;

std::string getFileExtension(const std::string& filename)
{
	uint pos = (uint)filename.find_last_of('.');
	return filename.substr(pos + 1);
}

std::string getAssetName(const std::string& srcFilename)
{
	uint startPos = (uint)strlen(Paths::GetSrcDataDir()) + 1;
	uint endPos = (uint)srcFilename.find_last_of('.');
	std::string assetName = srcFilename.substr(startPos, endPos - startPos);
	return assetName;
}

void createDirectoryTreeForFile(const std::string& filename)
{
	int endPos = -1;
	while ((endPos = (int)filename.find_first_of("/\\", endPos + 1)) != std::string::npos)
	{
		std::string dir = filename.substr(0, endPos);
		CreateDirectoryA(dir.c_str(), nullptr);
	}
}

void binAsset(const std::string& filename)
{
	std::string ext = getFileExtension(filename);

	// Shader special case.  Just copy the file.
	if (ext.compare("h") == 0 || ext.compare("hlsli") == 0 || ext.compare("hlsl") == 0)
	{
		std::string binFilename = Paths::GetBinDataDir() + ("/" + getAssetName(filename) + "." + ext);
		createDirectoryTreeForFile(binFilename);
		CopyFileA(filename.c_str(), binFilename.c_str(), false);
		printf("\tCopied shader %s\n", filename.c_str());
		return;
	}

	AssetBinnerMap::iterator binIter = g_assetBinners.find(ext);
	if (binIter != g_assetBinners.end())
	{
		AssetLib::AssetDef& rAssetDef = binIter->second->GetAssetDef();

		AssetLib::BinFileHeader header;
		header.binUID = AssetLib::BinFileHeader::kUID;
		header.assetUID = rAssetDef.GetAssetUID();
		header.version = rAssetDef.GetBinVersion();
		binIter->second->CalcSourceHash(filename, header.srcHash);

		// Determine bin file and create directory tree if necessary.
		std::string binFilename = Paths::GetBinDataDir() + ("/" + getAssetName(filename) + "." + rAssetDef.GetExt());
		createDirectoryTreeForFile(binFilename);

		// Compare existing bin file's hash with src data.
		std::string reason;
		{
			std::ifstream existingBinFile(binFilename, std::ios::binary);
			if (existingBinFile.is_open())
			{
				AssetLib::BinFileHeader existingHeader;
				existingBinFile.read((char*)&existingHeader, sizeof(AssetLib::BinFileHeader));

				if (header.version != existingHeader.version)
				{
					reason = "Version mismatch:  Got: " + existingHeader.version;
					reason += " Expected: " + header.version;
				}
				else if (memcmp(&existingHeader.srcHash, &header.srcHash, sizeof(Hashing::SHA1)) != 0)
				{
					reason = "Source SHA1 mismatch";
				}
				else
				{
					// Version and source hashes match, no need to bin.
					return;
				}
			}
			else
			{
				reason = "Bin data does not exist";
			}
		}

		printf("\tBinning %s...\n", filename.c_str());
		printf("\t  Reason: %s\n", reason.c_str());

		// Open bin file for writing and write the new header.
		std::ofstream binFile(binFilename, std::ios::binary);
		assert(binFile.is_open());
		binFile.write((char*)&header, sizeof(header));

		// Write asset specific bin data.
		binIter->second->BinAsset(filename, binFile);

		printf("\t  done\n");
	}
}

void fileChangedCallback(const char* filename, void* pUserData)
{
	std::string filePath = Paths::GetSrcDataDir();
	filePath += "\\";
	filePath += filename;
	binAsset(filePath);
}

void binAssetsInDirectory(const std::string& directory)
{
	std::string findPath = directory + "/*";

	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(findPath.c_str(), &findData);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do
	{
		if (findData.cFileName[0] == '.')
			continue;

		if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			std::string subdir = directory + "/" + findData.cFileName;
			binAssetsInDirectory(subdir);
		}
		else
		{
			binAsset(directory + "/" + findData.cFileName);
		}
	} while (FindNextFileA(hFind, &findData));
}

void registerAssetBinner(IAssetBinner* pBinner)
{
	std::vector<std::string> fileTypes = pBinner->GetFileTypes();
	for (int i = (int)fileTypes.size() - 1; i >= 0; --i)
	{
		assert(g_assetBinners.find(fileTypes[i]) == g_assetBinners.end());
		g_assetBinners.insert(std::make_pair(fileTypes[i], pBinner));
	}
}

int main(int argc, char** argv)
{
	// For DirectXTex WIC
	HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	assert(hr == S_OK);

	// Register asset binners
	registerAssetBinner(new ModelBinner());
	registerAssetBinner(new TextureBinner());
	registerAssetBinner(new SceneBinner());
	registerAssetBinner(new SkyBinner());
	registerAssetBinner(new MaterialBinner());
	registerAssetBinner(new PostProcessEffectsBinner());

	FileWatcher::Init(Paths::GetSrcDataDir());
	FileWatcher::AddListener("*.*", fileChangedCallback, nullptr);

	// Validate all assets
	printf("Pre-processing all source files...\n");
	binAssetsInDirectory(Paths::GetSrcDataDir());

	// Run until terminated manually.
	printf("Listening for file changes...\n");
	Sleep(INFINITE);
}