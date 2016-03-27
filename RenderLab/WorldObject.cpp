#include "Precompiled.h"
#include "WorldObject.h"
#include "render/Model.h"
#include "render/Renderer.h"
#include "render/Camera.h"

WorldObject::WorldObject(Model* pModel, Vec3 pos, Quaternion orientation, Vec3 scale)
	: m_pModel(pModel)
	, m_position(pos)
	, m_scale(scale)
	, m_orientation(orientation)
{
}


void WorldObject::QueueDraw(Renderer& rRenderer) const
{
	if (m_pModel)
	{
		RdrGeoHandle hGeo = m_pModel->GetGeoHandle();
		float radius = rRenderer.GetContext()->GetGeometry(hGeo)->radius * Vec3MaxComponent(m_scale);
		if (!rRenderer.GetCurrentCamera().CanSee(m_position, radius))
			return;

		m_pModel->QueueDraw(rRenderer, GetTransform());
	}
}