#pragma once

#include "FreeList.h"
#include "VolumeComponent.h"
#include "AssetLib\SceneAsset.h"

class SkyVolume : public VolumeComponent
{
public:
	static SkyVolume* Create(IComponentAllocator* pAllocator, const AssetLib::Volume& rVolume);

public:
	void Release();

	void OnAttached(Entity* pEntity);
	void OnDetached(Entity* pEntity);

	const AssetLib::SkySettings& GetSkySettings() const;
	AssetLib::SkySettings& GetSkySettings();

private:
	FRIEND_FREELIST;
	SkyVolume();
	SkyVolume(const SkyVolume&);
	virtual ~SkyVolume() {}

private:
	AssetLib::SkySettings m_sky;
};

inline const AssetLib::SkySettings& SkyVolume::GetSkySettings() const
{
	return m_sky;
}

inline AssetLib::SkySettings& SkyVolume::GetSkySettings()
{
	return m_sky;
}
