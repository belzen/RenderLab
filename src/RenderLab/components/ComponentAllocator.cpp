#include "Precompiled.h"
#include "ComponentAllocator.h"
#include "EntityComponent.h"

void IComponentAllocator::InitComponent(EntityComponent* pComponent)
{
	pComponent->m_pAllocator = this;
}

Light* DefaultComponentAllocator::AllocLight()
{
	Light* pComponent = m_lights.allocSafe();
	InitComponent(pComponent);
	return pComponent;
}

ModelInstance* DefaultComponentAllocator::AllocModelInstance()
{
	ModelInstance* pComponent = m_models.allocSafe();
	InitComponent(pComponent);
	return pComponent;
}

RigidBody* DefaultComponentAllocator::AllocRigidBody()
{
	RigidBody* pComponent = m_rigidBodies.allocSafe();
	InitComponent(pComponent);
	return pComponent;
}

SkyVolume* DefaultComponentAllocator::AllocSkyVolume()
{
	SkyVolume* pComponent = m_skyVolumes.allocSafe();
	InitComponent(pComponent);
	return pComponent;
}

PostProcessVolume* DefaultComponentAllocator::AllocPostProcessVolume()
{
	PostProcessVolume* pComponent = m_postProcVolumes.allocSafe();
	InitComponent(pComponent);
	return pComponent;
}

void DefaultComponentAllocator::ReleaseComponent(const Light* pComponent)
{
	return m_lights.release(pComponent);
}

void DefaultComponentAllocator::ReleaseComponent(const ModelInstance* pComponent)
{
	return m_models.release(pComponent);
}

void DefaultComponentAllocator::ReleaseComponent(const RigidBody* pComponent)
{
	return m_rigidBodies.release(pComponent);
}

void DefaultComponentAllocator::ReleaseComponent(const SkyVolume* pComponent)
{
	return m_skyVolumes.release(pComponent);
}

void DefaultComponentAllocator::ReleaseComponent(const PostProcessVolume* pComponent)
{
	return m_postProcVolumes.release(pComponent);
}

ModelInstanceFreeList& DefaultComponentAllocator::GetModelInstanceFreeList()
{
	return m_models;
}

LightFreeList& DefaultComponentAllocator::GetLightFreeList()
{
	return m_lights;
}

RigidBodyFreeList& DefaultComponentAllocator::GetRigidBodyFreeList()
{
	return m_rigidBodies;
}

PostProcessVolumeFreeList& DefaultComponentAllocator::GetPostProcessVolumeFreeList()
{
	return m_postProcVolumes;
}

SkyVolumeFreeList& DefaultComponentAllocator::GetSkyVolumeFreeList()
{
	return m_skyVolumes;
}
