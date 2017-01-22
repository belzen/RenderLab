#pragma once

#include "FreeList.h"
#include "VolumeComponent.h"
#include "AssetLib\SceneAsset.h"

class PostProcessVolume : public VolumeComponent
{
public:
	static PostProcessVolume* Create(IComponentAllocator* pAllocator, const AssetLib::Volume& rVolume);

public:
	void Release();

	void OnAttached(Entity* pEntity);
	void OnDetached(Entity* pEntity);

	const AssetLib::PostProcessEffects& GetEffects() const;
	AssetLib::PostProcessEffects& GetEffects();

private:
	FRIEND_FREELIST;
	PostProcessVolume();
	PostProcessVolume(const PostProcessVolume&);
	virtual ~PostProcessVolume() {}

private:
	AssetLib::PostProcessEffects m_effects;
};

inline const AssetLib::PostProcessEffects& PostProcessVolume::GetEffects() const
{
	return m_effects;
}

inline AssetLib::PostProcessEffects& PostProcessVolume::GetEffects()
{
	return m_effects;
}
