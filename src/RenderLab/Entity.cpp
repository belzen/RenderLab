#include "Precompiled.h"
#include "Entity.h"
#include "components/ModelInstance.h"
#include "components/RigidBody.h"
#include "components/Light.h"
#include "components/VolumeComponent.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "Physics.h"

namespace
{
	EntityFreeList s_entityFreeList;
}

EntityFreeList& Entity::GetFreeList()
{
	return s_entityFreeList;
}

Entity* Entity::Create(const char* name, Vec3 pos, Rotation rotation, Vec3 scale)
{
	Entity* pEntity = s_entityFreeList.allocSafe();

	strcpy_s(pEntity->m_name, name);
	pEntity->m_position = pos;
	pEntity->m_scale = scale;
	pEntity->SetRotation(rotation);
	pEntity->m_transformId = 1;

	return pEntity;
}

void Entity::AttachRigidBody(RigidBody* pRigidBody)
{
	if (m_pRigidBody)
	{
		m_pRigidBody->OnDetached(this);
		m_pRigidBody->Release();
	}

	m_pRigidBody = pRigidBody;

	if (m_pRigidBody)
	{
		m_pRigidBody->OnAttached(this);
	}
}

void Entity::AttachRenderable(Renderable* pRenderable)
{
	if (m_pRenderable)
	{
		m_pRenderable->OnDetached(this);
		m_pRenderable->Release();
	}

	m_pRenderable = pRenderable;

	if (m_pRenderable)
	{
		m_pRenderable->OnAttached(this);
	}
}

void Entity::AttachLight(Light* pLight)
{
	if (m_pLight)
	{
		m_pLight->OnDetached(this);
		m_pLight->Release();
	}

	m_pLight = pLight;

	if (pLight)
	{
		m_pLight->OnAttached(this);
	}
}

void Entity::AttachVolume(VolumeComponent* pVolume)
{
	if (m_pVolume)
	{
		m_pVolume->OnDetached(this);
		m_pVolume->Release();
	}

	m_pVolume = pVolume;

	if (m_pVolume)
	{
		m_pVolume->OnAttached(this);
	}
}

void Entity::Release()
{
	if (m_pRigidBody)
	{
		m_pRigidBody->Release();
		m_pRigidBody = nullptr;
	}

	if (m_pRenderable)
	{
		m_pRenderable->Release();
		m_pRenderable = nullptr;
	}

	if (m_pLight)
	{
		m_pLight->Release();
		m_pLight = nullptr;
	}

	if (m_pVolume)
	{
		m_pVolume->Release();
		m_pVolume = nullptr;
	}

	s_entityFreeList.releaseSafe(this);
}
