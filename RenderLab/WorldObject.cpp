#include "Precompiled.h"
#include "WorldObject.h"
#include "render/Model.h"
#include "render/Renderer.h"
#include "render/Camera.h"

namespace
{
	WorldObjectList s_worldObjects;
}

WorldObject* WorldObject::Create(Model* pModel, Vec3 pos, Quaternion orientation, Vec3 scale)
{
	WorldObject* pObject = s_worldObjects.allocSafe();

	pObject->m_pModel = pModel;
	pObject->m_position = pos;
	pObject->m_scale = scale;
	pObject->m_orientation = orientation;

	return pObject;
}

void WorldObject::Release()
{
	m_pModel->Release();
	s_worldObjects.releaseSafe(this);
}

void WorldObject::QueueDraw(Renderer& rRenderer) const
{
	if (m_pModel)
	{
		float radius = m_pModel->GetRadius() * Vec3MaxComponent(m_scale);
		if (!rRenderer.GetCurrentCamera().CanSee(m_position, radius))
			return;

		m_pModel->QueueDraw(rRenderer, GetTransform());
	}
}