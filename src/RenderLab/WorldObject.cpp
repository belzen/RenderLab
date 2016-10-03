#include "Precompiled.h"
#include "WorldObject.h"
#include "render/ModelInstance.h"
#include "render/Renderer.h"
#include "render/Camera.h"

namespace
{
	WorldObjectFreeList s_worldObjects;
}

WorldObject* WorldObject::Create(const char* name, ModelInstance* pModel, Vec3 pos, Quaternion orientation, Vec3 scale)
{
	WorldObject* pObject = s_worldObjects.allocSafe();

	strcpy_s(pObject->m_name, name);
	pObject->m_pModel = pModel;
	pObject->m_position = pos;
	pObject->m_scale = scale;
	pObject->m_orientation = orientation;
	pObject->m_transformChanged = true;

	return pObject;
}

void WorldObject::Release()
{
	m_pModel->Release();
	s_worldObjects.releaseSafe(this);
}

void WorldObject::PrepareDraw()
{
	if (m_pModel)
	{
		m_pModel->PrepareDraw(GetTransform(), m_transformChanged);
		m_transformChanged = false;
	}
}