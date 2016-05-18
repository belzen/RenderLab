#pragma once
#include <fstream>
#include <vector>
#include <string>
#include "../Types.h"
#include "AssetLib/BinFile.h"
#include "AssetLib/AssetDef.h"

class IAssetBinner
{
public:
	virtual AssetLib::AssetDef& GetAssetDef() const = 0;

	virtual std::vector<std::string> GetFileTypes() const = 0;

	virtual void CalcSourceHash(const std::string& srcFilename, Hashing::SHA1& rOutHash) = 0;

	// Bin asset and write results into the dstFile.
	virtual bool BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const = 0;
};