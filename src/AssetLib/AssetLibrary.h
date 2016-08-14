#pragma once

#include "UtilsLib/Error.h"

template<typename AssetTypeT>
class AssetLibrary
{
public:
	typedef std::map<Hashing::StringHash, AssetTypeT*> AssetMap;

	static AssetTypeT* LoadAsset(const char* assetName)
	{
		return GetInstance()->LoadAssetInternal(assetName);
	}

private:
	static AssetLibrary<AssetTypeT>* GetInstance()
	{
		if (!s_pInstance)
		{
			s_pInstance = new AssetLibrary<AssetTypeT>();
		}
		return s_pInstance;
	}

	AssetTypeT* LoadAssetInternal(const char* assetName)
	{
		Hashing::StringHash nameHash = Hashing::HashString(assetName);
		AssetMap::iterator iter = m_assetCache.find(nameHash);
		if (iter != m_assetCache.end())
			return iter->second;

		char* pFileData = nullptr;
		uint fileSize = 0;
		if (!AssetTypeT::s_definition.LoadAsset(assetName, &pFileData, &fileSize))
		{
			Error("Failed to load asset: %s", assetName);
			return nullptr;
		}

		AssetTypeT* pAsset = AssetTypeT::FromMem(pFileData);
		pAsset->assetName = _strdup(assetName);
		m_assetCache.insert(std::make_pair(nameHash, pAsset));
		return pAsset;
	}

private:
	static AssetLibrary<AssetTypeT>* s_pInstance;

	AssetMap m_assetCache;
};

template<typename AssetTypeT>
AssetLibrary<AssetTypeT>* AssetLibrary<AssetTypeT>::s_pInstance = nullptr;