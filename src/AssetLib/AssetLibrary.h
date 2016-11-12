#pragma once

#include <set>
#include "UtilsLib/Error.h"
#include "UtilsLib/JsonUtils.h"
#include "UtilsLib/Hash.h"
#include "UtilsLib/FileWatcher.h"

template<typename AssetTypeT>
class IAssetReloadListener
{
public:
	virtual void OnAssetReloaded(const AssetTypeT* pAsset) = 0;
};

template<typename AssetTypeT>
class AssetLibrary
{
public:
	typedef std::map<Hashing::StringHash, AssetTypeT*> AssetMap;

	static AssetTypeT* LoadAsset(const char* assetName)
	{
		return GetInstance()->LoadAssetInternal(assetName);
	}

	static void AddReloadListener(IAssetReloadListener<AssetTypeT>* pListener)
	{
		GetInstance()->m_reloadListeners.insert(pListener);
	}

	static void RemoveReloadListener(IAssetReloadListener<AssetTypeT>* pListener)
	{
		GetInstance()->m_reloadListeners.erase(pListener);
	}

private:
	AssetLibrary<AssetTypeT>()
		: m_reloadListenerId(0) {}

	static AssetLibrary<AssetTypeT>* GetInstance()
	{
		if (!s_pInstance)
		{
			s_pInstance = new AssetLibrary<AssetTypeT>();
		}
		return s_pInstance;
	}

	static void HandleAssetFileChanged(const char* filename, void* pUserData)
	{
		char assetName[AssetLib::AssetDef::kMaxNameLen];
		AssetTypeT::GetAssetDef().ExtractAssetName(filename, assetName, ARRAY_SIZE(assetName));

		GetInstance()->ReloadAssetInternal(assetName);
	}

	AssetTypeT* LoadAssetInternal(const char* assetName)
	{
		Hashing::StringHash nameHash = Hashing::HashString(assetName);
		AssetMap::iterator iter = m_assetCache.find(nameHash);
		if (iter != m_assetCache.end())
			return iter->second;

		if (!m_reloadListenerId)
		{
			char filePattern[AssetLib::AssetDef::kMaxNameLen];
			AssetTypeT::GetAssetDef().GetFilePattern(filePattern, ARRAY_SIZE(filePattern));
			m_reloadListenerId = FileWatcher::AddListener(filePattern, HandleAssetFileChanged, this);
		}

		AssetTypeT* pAsset = AssetTypeT::Load(assetName, nullptr);
		pAsset->assetName = _strdup(assetName);
		m_assetCache.insert(std::make_pair(nameHash, pAsset));
		return pAsset;
	}

	AssetTypeT* ReloadAssetInternal(const char* assetName)
	{
		Hashing::StringHash nameHash = Hashing::HashString(assetName);
		AssetMap::iterator iter = m_assetCache.find(nameHash);
		if (iter == m_assetCache.end())
			return nullptr; // File hasn't been loaded, nothing to reload

		iter->second = AssetTypeT::Load(assetName, iter->second);

		for (IAssetReloadListener<AssetTypeT>* pListener : m_reloadListeners)
		{
			pListener->OnAssetReloaded(iter->second);
		}

		return iter->second;
	}

private:
	static AssetLibrary<AssetTypeT>* s_pInstance;

	AssetMap m_assetCache;
	std::set<IAssetReloadListener<AssetTypeT>*> m_reloadListeners;

	FileWatcher::ListenerID m_reloadListenerId;
};

template<typename AssetTypeT>
AssetLibrary<AssetTypeT>* AssetLibrary<AssetTypeT>::s_pInstance = nullptr;