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

void WorldObject::SetRigidBody(RigidBody* pRigidBody)
{
	assert(!m_pRigidBody);
	m_pRigidBody = pRigidBody;
	if (m_pRigidBody)
	{
		m_pRigidBody->AddToScene(m_position, m_orientation);
	}
}

void WorldObject::SetModel(ModelInstance* pModel)
{
	if (m_pModel)
	{
		m_pModel->Release();
	}

	m_pModel = pModel;
}

void WorldObject::Release()
{
	m_pModel->Release();
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