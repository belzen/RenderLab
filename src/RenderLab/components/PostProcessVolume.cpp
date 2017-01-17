#include "Precompiled.h"
#include "PostProcessVolume.h"

namespace
{
	PostProcessVolumeFreeList s_postProcessVolumeFreeList;
}

PostProcessVolumeFreeList& PostProcessVolume::GetFreeList()
{
	return s_postProcessVolumeFreeList;
}

PostProcessVolume* PostProcessVolume::Create(const AssetLib::Volume& rVolume)
{
	assert(rVolume.volumeType == AssetLib::VolumeType::kPostProcess);

	PostProcessVolume* pPostProcess = s_postProcessVolumeFreeList.allocSafe();
	pPostProcess->m_effects = rVolume.postProcessEffects;
	return pPostProcess;
}

PostProcessVolume::PostProcessVolume()
{

}

void PostProcessVolume::Release()
{
	s_postProcessVolumeFreeList.release(this);
}

void PostProcessVolume::OnAttached(Entity* pEntity)
{
	m_pEntity = pEntity;
}

void PostProcessVolume::OnDetached(Entity* pEntity)
{
	m_pEntity = nullptr;
}
