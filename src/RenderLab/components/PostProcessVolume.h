#pragma once

#include "FreeList.h"
#include "VolumeComponent.h"
#include "AssetLib\SceneAsset.h"

class PostProcessVolume;
typedef FreeList<PostProcessVolume, 128> PostProcessVolumeFreeList;

class PostProcessVolume : public VolumeComponent
{
public:
	static PostProcessVolumeFreeList& GetFreeList();
	static PostProcessVolume* Create(const AssetLib::Volume& rVolume);

public:
	void Release();

	void OnAttached(Entity* pEntity);
	void OnDetached(Entity* pEntity);

	const AssetLib::PostProcessEffects& GetEffects() const;
	AssetLib::PostProcessEffects& GetEffects();

private:
	friend PostProcessVolumeFreeList;
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
