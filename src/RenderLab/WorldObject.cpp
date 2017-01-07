#include "Precompiled.h"
#include "WorldObject.h"
#include "components/ModelInstance.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "Physics.h"

namespace
{
	WorldObjectFreeList s_worldObjects;
}

WorldObject* WorldObject::Create(const char* name, Vec3 pos, Quaternion orientation, Vec3 scale)
{
	WorldObject* pObject = s_worldObjects.allocSafe();

	strcpy_s(pObject->m_name, name);
	pObject->m_position = pos;
	pObject->m_scale = scale;
	pObject->m_orientation = orientation;
	pObject->m_transformChanged = true;

	return pObject;
}

void WorldObject::AttachRigidBody(RigidBody* pRigidBody)
{
	if (m_pRigidBody)
	{
		m_pRigidBody->OnDetached(this);
		m_pRigidBody->Release();
	}

	m_pRigidBody = pRigidBody;
	m_pRigidBody->OnAttached(this);
}

void WorldObject::AttachModel(ModelInstance* pModel)
{
	if (m_pModel)
	{
		m_pModel->OnDetached(this);
		m_pModel->Release();
	}

	m_pModel = pModel;
	m_pModel->OnAttached(this);
}

void WorldObject::Release()
{
	if (m_pRigidBody)
	{
		m_pRigidBody->Release();
		m_pRigidBody = nullptr;
	}

	if (m_pModel)
	{
		m_pModel->Release();
		m_pModel = nullptr;
	}

	s_worldObjects.releaseSafe(this);
}

void WorldObject::Update()
{
	if (m_pRigidBody)
	{
		m_position = m_pRigidBody->GetPosition();
		m_orientation = m_pRigidBody->GetOrientation();
		m_transformChanged = true; // Todo: This is only necessary if the object actually moved.
	}
}

void WorldObject::QueueDraw(RdrDrawBuckets* pDrawBuckets)
{
	if (m_pModel)
	{
		m_pModel->QueueDraw(pDrawBuckets, GetTransform(), m_transformChanged);
		m_transformChanged = false;
	}
}