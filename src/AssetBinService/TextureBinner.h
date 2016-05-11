#pragma once

#include "IAssetBinner.h"

class TextureBinner : public IAssetBinner
{
public:
	int GetVersion() const;

	int GetAssetUID() const;

	std::string GetBinExtension() const;

	std::vector<std::string> GetFileTypes() const;

	void CalcSourceHash(const std::string& srcFilename, SHA1Hash& rOutHash);

	bool BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const;
};
