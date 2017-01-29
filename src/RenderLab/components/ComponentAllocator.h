#pragma once

#include "FreeList.h"
#include "RigidBody.h"
#include "Light.h"
#include "ModelComponent.h"
#include "PostProcessVolume.h"
#include "SkyVolume.h"

class IComponentAllocator
{
public:
	virtual Light* AllocLight() = 0;
	virtual ModelComponent* AllocModelComponent() = 0;
	virtual RigidBody* AllocRigidBody() = 0;
	virtual SkyVolume* AllocSkyVolume() = 0;
	virtual PostProcessVolume* AllocPostProcessVolume() = 0;

	virtual void ReleaseComponent(const Light* pComponent) = 0;
	virtual void ReleaseComponent(const ModelComponent* pComponent) = 0;
	virtual void ReleaseComponent(const RigidBody* pComponent) = 0;
	virtual void ReleaseComponent(const SkyVolume* pComponent) = 0;
	virtual void ReleaseComponent(const PostProcessVolume* pComponent) = 0;

	void InitComponent(EntityComponent* pComponent);
};

typedef FreeList<Light, 6 * 1024> LightFreeList;
typedef FreeList<ModelComponent, 6 * 1024> ModelComponentFreeList;
typedef FreeList<RigidBody, 6 * 1024> RigidBodyFreeList;
typedef FreeList<PostProcessVolume, 128> PostProcessVolumeFreeList;
typedef FreeList<SkyVolume, 128> SkyVolumeFreeList;

class DefaultComponentAllocator : public IComponentAllocator
{
public:
	Light* AllocLight();
	ModelComponent* AllocModelComponent();
	RigidBody* AllocRigidBody();
	SkyVolume* AllocSkyVolume();
	PostProcessVolume* AllocPostProcessVolume();

	void ReleaseComponent(const Light* pComponent);
	void ReleaseComponent(const ModelComponent* pComponent);
	void ReleaseComponent(const RigidBody* pComponent);
	void ReleaseComponent(const SkyVolume* pComponent);
	void ReleaseComponent(const PostProcessVolume* pComponent);

	ModelComponentFreeList& GetModelComponentFreeList();
	LightFreeList& GetLightFreeList();
	RigidBodyFreeList& GetRigidBodyFreeList();
	PostProcessVolumeFreeList& GetPostProcessVolumeFreeList();
	SkyVolumeFreeList& GetSkyVolumeFreeList();

private:
	ModelComponentFreeList m_models;
	LightFreeList m_lights;
	RigidBodyFreeList m_rigidBodies;
	PostProcessVolumeFreeList m_postProcVolumes;
	SkyVolumeFreeList m_skyVolumes;
};