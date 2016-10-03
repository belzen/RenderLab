#include "Precompiled.h"
#include "WorldObjectViewModel.h"
#include "WorldObject.h"
#include "PropertyTables.h"

void WorldObjectViewModel::SetTarget(WorldObject* pObject)
{
	m_pObject = pObject;
}

const PropertyDef** WorldObjectViewModel::GetProperties()
{
	static const PropertyDef* s_ppProperties[] = {
		new TextPropertyDef("Name", "", GetName, SetName),
		new Vector3PropertyDef("Position", "", GetPosition, SetPosition, SetPositionX, SetPositionY, SetPositionZ),
		new Vector3PropertyDef("Scale", "", GetScale, SetScale, SetScaleX, SetScaleY, SetScaleZ),
		new ModelPropertyDef("Model", "", GetModel, SetModel),
		nullptr
	};
	return s_ppProperties;
}

std::string WorldObjectViewModel::GetName(void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	return pViewModel->m_pObject->GetName();
}

bool WorldObjectViewModel::SetName(const std::string& name, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	pViewModel->m_pObject->SetName(name.c_str());
	return true;
}

std::string WorldObjectViewModel::GetModel(void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	return pViewModel->m_pObject->GetModel()->GetModelData()->GetName();
}

bool WorldObjectViewModel::SetModel(const std::string& modelName, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	ModelInstance* pModel = ModelInstance::Create(modelName.c_str());
	pViewModel->m_pObject->SetModel(pModel);
	return true;
}

Vec3 WorldObjectViewModel::GetPosition(void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	return pViewModel->m_pObject->GetPosition();
}

bool WorldObjectViewModel::SetPosition(const Vec3& position, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	pViewModel->m_pObject->SetPosition(position);
	return true;
}

bool WorldObjectViewModel::SetPositionX(const float newValue, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	Vec3 pos = pViewModel->m_pObject->GetPosition();
	pos.x = newValue;
	pViewModel->m_pObject->SetPosition(pos);
	return true;
}

bool WorldObjectViewModel::SetPositionY(const float newValue, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	Vec3 pos = pViewModel->m_pObject->GetPosition();
	pos.y = newValue;
	pViewModel->m_pObject->SetPosition(pos);
	return true;
}

bool WorldObjectViewModel::SetPositionZ(const float newValue, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	Vec3 pos = pViewModel->m_pObject->GetPosition();
	pos.z = newValue;
	pViewModel->m_pObject->SetPosition(pos);
	return true;
}

Vec3 WorldObjectViewModel::GetScale(void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	return pViewModel->m_pObject->GetScale();
}

bool WorldObjectViewModel::SetScale(const Vec3& scale, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	pViewModel->m_pObject->SetScale(scale);
	return true;
}

bool WorldObjectViewModel::SetScaleX(const float newValue, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	Vec3 scale = pViewModel->m_pObject->GetScale();
	scale.x = newValue;
	pViewModel->m_pObject->SetScale(scale);
	return true;
}

bool WorldObjectViewModel::SetScaleY(const float newValue, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	Vec3 scale = pViewModel->m_pObject->GetScale();
	scale.y = newValue;
	pViewModel->m_pObject->SetScale(scale);
	return true;
}

bool WorldObjectViewModel::SetScaleZ(const float newValue, void* pSource)
{
	WorldObjectViewModel* pViewModel = (WorldObjectViewModel*)pSource;
	Vec3 scale = pViewModel->m_pObject->GetScale();
	scale.z = newValue;
	pViewModel->m_pObject->SetScale(scale);
	return true;
}
