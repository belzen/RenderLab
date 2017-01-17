#include "Precompiled.h"
#include "SkyVolume.h"
#include "render/ModelData.h"

namespace
{
	SkyVolumeFreeList s_skyVolumeFreeList;
}

SkyVolumeFreeList& SkyVolume::GetFreeList()
{
	return s_skyVolumeFreeList;
}

SkyVolume* SkyVolume::Create(const AssetLib::Volume& rVolume)
{
	assert(rVolume.volumeType == AssetLib::VolumeType::kSky);

	SkyVolume* pSky = s_skyVolumeFreeList.allocSafe();
	pSky->m_sky = rVolume.skySettings;
	return pSky;
}

SkyVolume::SkyVolume()
{

}

void SkyVolume::Release()
{
	s_skyVolumeFreeList.release(this);
}

void SkyVolume::OnAttached(Entity* pEntity)
{
	m_pEntity = pEntity;
}

void SkyVolume::OnDetached(Entity* pEntity)
{
	m_pEntity = nullptr;
}

