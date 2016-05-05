#pragma once
#include <fstream>
#include <vector>
#include <string>
#include "../Types.h"
#include "AssetLib/BinFile.h"

class IAssetBinner
{
public:
	virtual int GetVersion() const = 0;

	virtual int GetAssetUID() const = 0;

	virtual std::string GetBinExtension() const = 0;

	virtual std::vector<std::string> GetFileTypes() const = 0;

	virtual void CalcSourceHash(const std::string& srcFilename, SHA1Hash& rOutHash) = 0;

	// Bin asset and write results into the dstFile.
	virtual bool BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const = 0;
};