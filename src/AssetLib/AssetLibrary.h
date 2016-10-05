#pragma once

#include "UtilsLib/Error.h"
#include "UtilsLib/JsonUtils.h"
#include "UtilsLib/Hash.h"

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

		AssetTypeT* pAsset = AssetTypeT::Load(assetName);
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