#include "Precompiled.h"
#include "EntityViewModel.h"
#include "Entity.h"
#include "PropertyTables.h"
#include "components/ModelComponent.h"

void EntityViewModel::SetTarget(Entity* pObject)
{
	m_pEntity = pObject;
}

const char* EntityViewModel::GetTypeName()
{
	return "Entity";
}

const PropertyDef** EntityViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
		new TextPropertyDef("Name", "", GetName, SetName),
		new Vector3PropertyDef("Position", "", GetPosition, SetPosition, SetPositionX, SetPositionY, SetPositionZ),
		new RotationPropertyDef("Rotation", "", GetRotation, SetRotation, SetRotationPitch, SetRotationYaw, SetRotationRoll),
		new Vector3PropertyDef("Scale", "", GetScale, SetScale, SetScaleX, SetScaleY, SetScaleZ),
		nullptr
	};
	return s_ppProperties;
}

std::string EntityViewModel::GetName(void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	return pViewModel->m_pEntity->GetName();
}

bool EntityViewModel::SetName(const std::string& name, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	pViewModel->m_pEntity->SetName(name.c_str());
	return true;
}

Vec3 EntityViewModel::GetPosition(void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	return pViewModel->m_pEntity->GetPosition();
}

bool EntityViewModel::SetPosition(const Vec3& position, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	pViewModel->m_pEntity->SetPosition(position);
	return true;
}

bool EntityViewModel::SetPositionX(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Vec3 pos = pViewModel->m_pEntity->GetPosition();
	pos.x = newValue;
	pViewModel->m_pEntity->SetPosition(pos);
	return true;
}

bool EntityViewModel::SetPositionY(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Vec3 pos = pViewModel->m_pEntity->GetPosition();
	pos.y = newValue;
	pViewModel->m_pEntity->SetPosition(pos);
	return true;
}

bool EntityViewModel::SetPositionZ(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Vec3 pos = pViewModel->m_pEntity->GetPosition();
	pos.z = newValue;
	pViewModel->m_pEntity->SetPosition(pos);
	return true;
}

Rotation EntityViewModel::GetRotation(void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Rotation r = pViewModel->m_pEntity->GetRotation();
	r.pitch = Maths::RadToDeg(r.pitch);
	r.yaw = Maths::RadToDeg(r.yaw);
	r.roll = Maths::RadToDeg(r.roll);
	return r;
}

bool EntityViewModel::SetRotation(const Rotation& rotation, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Rotation r = rotation;
	r.pitch = Maths::DegToRad(rotation.pitch);
	r.yaw = Maths::DegToRad(rotation.yaw);
	r.roll = Maths::DegToRad(rotation.roll);
	pViewModel->m_pEntity->SetRotation(r);
	return true;
}

bool EntityViewModel::SetRotationPitch(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Rotation r = pViewModel->m_pEntity->GetRotation();
	r.pitch = Maths::DegToRad(newValue);
	pViewModel->m_pEntity->SetRotation(r);
	return true;
}

bool EntityViewModel::SetRotationYaw(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Rotation r = pViewModel->m_pEntity->GetRotation();
	r.yaw = Maths::DegToRad(newValue);
	pViewModel->m_pEntity->SetRotation(r);
	return true;
}

bool EntityViewModel::SetRotationRoll(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Rotation r = pViewModel->m_pEntity->GetRotation();
	r.roll = Maths::DegToRad(newValue);
	pViewModel->m_pEntity->SetRotation(r);
	return true;
}

Vec3 EntityViewModel::GetScale(void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	return pViewModel->m_pEntity->GetScale();
}

bool EntityViewModel::SetScale(const Vec3& scale, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	pViewModel->m_pEntity->SetScale(scale);
	return true;
}

bool EntityViewModel::SetScaleX(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Vec3 scale = pViewModel->m_pEntity->GetScale();
	scale.x = newValue;
	pViewModel->m_pEntity->SetScale(scale);
	return true;
}

bool EntityViewModel::SetScaleY(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Vec3 scale = pViewModel->m_pEntity->GetScale();
	scale.y = newValue;
	pViewModel->m_pEntity->SetScale(scale);
	return true;
}

bool EntityViewModel::SetScaleZ(const float newValue, void* pSource)
{
	EntityViewModel* pViewModel = (EntityViewModel*)pSource;
	Vec3 scale = pViewModel->m_pEntity->GetScale();
	scale.z = newValue;
	pViewModel->m_pEntity->SetScale(scale);
	return true;
}
