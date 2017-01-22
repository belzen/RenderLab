#include "Precompiled.h"
#include "Light.h"
#include "Entity.h"
#include "ComponentAllocator.h"

Light* Light::CreateDirectional(IComponentAllocator* pAllocator, const Vec3& color, float intensity, float pssmLambda)
{
	Light* pLight = pAllocator->AllocLight();
	pLight->m_type = LightType::Directional;
	pLight->m_colorRgb = color;
	pLight->m_intensity = intensity;
	pLight->m_color = pLight->m_intensity * pLight->m_colorRgb;
	pLight->m_pssmLambda = pssmLambda;
	return pLight;
}

Light* Light::CreateSpot(IComponentAllocator* pAllocator, const Vec3& color, float intensity, float radius, float innerConeAngle, float outerConeAngle)
{
	Light* pLight = pAllocator->AllocLight();
	pLight->m_type = LightType::Spot;
	pLight->m_colorRgb = color;
	pLight->m_intensity = intensity;
	pLight->m_color = pLight->m_intensity * pLight->m_colorRgb;
	pLight->m_radius = radius;
	pLight->m_innerConeAngle = innerConeAngle;
	pLight->m_innerConeAngleCos = cosf(innerConeAngle);
	pLight->m_outerConeAngle = outerConeAngle;
	pLight->m_outerConeAngleCos = cosf(outerConeAngle);
	return pLight;
}

Light* Light::CreatePoint(IComponentAllocator* pAllocator, const Vec3& color, float intensity, const float radius)
{
	Light* pLight = pAllocator->AllocLight();
	pLight->m_type = LightType::Point;
	pLight->m_colorRgb = color;
	pLight->m_intensity = intensity;
	pLight->m_color = pLight->m_intensity * pLight->m_colorRgb;
	pLight->m_radius = radius;
	return pLight;
}

Light* Light::CreateEnvironment(IComponentAllocator* pAllocator, bool isGlobal)
{
	Light* pLight = pAllocator->AllocLight();
	pLight->m_type = LightType::Environment;
	pLight->m_environmentTextureIndex = -1;
	pLight->m_isGlobalEnvironmentLight = isGlobal;
	return pLight;
}

void Light::OnAttached(Entity* pEntity)
{
	m_pEntity = pEntity;
}

void Light::OnDetached(Entity* pEntity)
{
	m_pEntity = nullptr;
}

void Light::Release()
{
	m_pAllocator->ReleaseComponent(this);
}

void Light::SetEnvironmentTextureIndex(int index)
{
	m_environmentTextureIndex = index;
}

void Light::SetType(LightType lightType)
{
	m_type = lightType;
}

void Light::SetColorRgb(const Vec3& color)
{
	m_colorRgb = color;
	m_color = m_colorRgb * m_intensity;
}

void Light::SetIntensity(float intensity)
{
	m_intensity = intensity;
	m_color = m_colorRgb * m_intensity;
}

void Light::SetRadius(float radius)
{
	m_radius = radius;
}

void Light::SetPssmLambda(float lambda)
{
	m_pssmLambda = lambda;
}

void Light::SetInnerConeAngle(float angle)
{
	m_innerConeAngle = angle;
	m_innerConeAngleCos = cosf(angle);
}

void Light::SetOuterConeAngle(float angle)
{
	m_outerConeAngle = angle;
	m_outerConeAngleCos = cosf(angle);
}

Vec3 Light::GetDirection() const
{
	const Entity* pEntity = GetEntity();
	Vec3 dir = Vec3(0.f, -1.f, 0.f);
	return Vec3Rotate(dir, pEntity->GetOrientation());
}
