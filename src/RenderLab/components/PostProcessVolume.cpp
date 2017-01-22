#include "Precompiled.h"
#include "PostProcessVolume.h"
#include "ComponentAllocator.h"

PostProcessVolume* PostProcessVolume::Create(IComponentAllocator* pAllocator, const AssetLib::Volume& rVolume)
{
	assert(rVolume.volumeType == AssetLib::VolumeType::kPostProcess);

	PostProcessVolume* pPostProcess = pAllocator->AllocPostProcessVolume();
	pPostProcess->m_effects = rVolume.postProcessEffects;
	return pPostProcess;
}

PostProcessVolume::PostProcessVolume()
{

}

void PostProcessVolume::Release()
{
	m_pAllocator->ReleaseComponent(this);
}

void PostProcessVolume::OnAttached(Entity* pEntity)
{
	m_pEntity = pEntity;
}

void PostProcessVolume::OnDetached(Entity* pEntity)
{
	m_pEntity = nullptr;
}
