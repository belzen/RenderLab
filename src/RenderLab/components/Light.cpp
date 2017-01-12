#include "Precompiled.h"
#include "Light.h"

namespace
{
	LightFreeList s_lights;
}

LightFreeList& Light::GetFreeList()
{
	return s_lights;
}

Light* Light::CreateDirectional(const Vec3& color, const Vec3& direction)
{
	Light* pLight = s_lights.allocSafe();
	pLight->m_type = LightType::Directional;
	pLight->m_color = color;
	pLight->m_direction = direction;
	return pLight;
}

Light* Light::CreateSpot(const Vec3& color, const Vec3& direction, float radius, float innerConeAngle, float outerConeAngle)
{
	Light* pLight = s_lights.allocSafe();
	pLight->m_type = LightType::Spot;
	pLight->m_color = color;
	pLight->m_direction = direction;
	pLight->m_radius = radius;
	pLight->m_innerConeAngle = innerConeAngle;
	pLight->m_innerConeAngleCos = cosf(innerConeAngle);
	pLight->m_outerConeAngle = outerConeAngle;
	pLight->m_outerConeAngleCos = cosf(outerConeAngle);
	return pLight;
}

Light* Light::CreatePoint(const Vec3& color, const float radius)
{
	Light* pLight = s_lights.allocSafe();
	pLight->m_type = LightType::Point;
	pLight->m_color = color;
	pLight->m_radius = radius;
	return pLight;
}

Light* Light::CreateEnvironment(bool isGlobal)
{
	Light* pLight = s_lights.allocSafe();
	pLight->m_type = LightType::Environment;
	pLight->m_environmentTextureIndex = -1;
	pLight->m_isGlobalEnvironmentLight = isGlobal;
	return pLight;
}

void Light::OnAttached(WorldObject* pObject)
{
	m_pParentObject = pObject;
}

void Light::OnDetached(WorldObject* pObject)
{
	m_pParentObject = nullptr;
}

void Light::Release()
{
}

void Light::SetEnvironmentTextureIndex(int index)
{
	m_environmentTextureIndex = index;
}
