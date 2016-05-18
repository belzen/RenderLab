#pragma once

#include "IAssetBinner.h"

class PostProcessEffectsBinner : public IAssetBinner
{
public:
	AssetLib::AssetDef& GetAssetDef() const;

	std::vector<std::string> GetFileTypes() const;

	void CalcSourceHash(const std::string& srcFilename, Hashing::SHA1& rOutHash);

	bool BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const;
};