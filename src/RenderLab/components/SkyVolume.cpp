#include "Precompiled.h"
#include "SkyVolume.h"
#include "render/ModelData.h"
#include "ComponentAllocator.h"

SkyVolume* SkyVolume::Create(IComponentAllocator* pAllocator, const AssetLib::Volume& rVolume)
{
	assert(rVolume.volumeType == AssetLib::VolumeType::kSky);

	SkyVolume* pSky = pAllocator->AllocSkyVolume();
	pSky->m_sky = rVolume.skySettings;
	return pSky;
}

SkyVolume::SkyVolume()
{

}

void SkyVolume::Release()
{
	m_pAllocator->ReleaseComponent(this);
}

void SkyVolume::OnAttached(Entity* pEntity)
{
	m_pEntity = pEntity;
}

void SkyVolume::OnDetached(Entity* pEntity)
{
	m_pEntity = nullptr;
}

