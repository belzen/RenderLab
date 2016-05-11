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
	while ( (endPos = (int)filename.find_first_of("/\\", endPos + 1)) != std::string::npos)
	{
		std::string dir = filename.substr(0, endPos);
		CreateDirectoryA(dir.c_str(), nullptr);
	}
}

void binAsset(const std::string& filename)
{
	std::string ext = getFileExtension(filename);
	AssetBinnerMap::iterator binIter = g_assetBinners.find(ext);
	if (binIter != g_assetBinners.end())
	{
		BinFileHeader header;
		header.binUID = BinFileHeader::kUID;
		header.assetUID = binIter->second->GetAssetUID();
		header.version = binIter->second->GetVersion();
		binIter->second->CalcSourceHash(filename, header.srcHash);

		// Determine bin file and create directory tree if necessary.
		std::string binFilename = Paths::GetBinDataDir() + ("/" + getAssetName(filename) + "." + binIter->second->GetBinExtension());
		createDirectoryTreeForFile(binFilename);

		// Compare existing bin file's hash with src data.
		{
			std::ifstream existingBinFile(binFilename, std::ios::binary);
			if (existingBinFile.is_open())
			{
				BinFileHeader existingHeader;
				existingBinFile.read((char*)&existingHeader, sizeof(BinFileHeader));

				if (header.version == existingHeader.version && memcmp(&existingHeader.srcHash, &header.srcHash, sizeof(SHA1Hash)) == 0)
				{
					// Version and source hashes match, no need to bin.
					return;
				}
			}
		}

		// Open bin file for writing and write the new header.
		std::ofstream binFile(binFilename, std::ios::binary);
		assert(binFile.is_open());
		binFile.write((char*)&header, sizeof(header));

		// Write asset specific bin data.
		binIter->second->BinAsset(filename, binFile);
	}
}

void fileChangedCallback(const char* filename, void* pUserData)
{
	// todo: handle file changes
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
	// Register asset binners
	registerAssetBinner(new ModelBinner());
	registerAssetBinner(new TextureBinner());
	registerAssetBinner(new SceneBinner());

	FileWatcher::Init(Paths::GetSrcDataDir());
	FileWatcher::AddListener(".*", fileChangedCallback, nullptr);

	// Validate all assets
	printf("Pre-processing all source files...\n");
	binAssetsInDirectory(Paths::GetSrcDataDir());

	// Run until terminated manually.
	printf("Listening for file changes...\n");
	while (true);
}