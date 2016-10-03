#include "Precompiled.h"
#include "SceneViewModel.h"
#include "Scene.h"
#include "PropertyTables.h"

void SceneViewModel::Init(Scene* pScene, AddedObjectFunc addedObjectCallback, void* pUserData)
{
	m_pScene = pScene;
	m_addedObjectCallback = addedObjectCallback;
	m_pUserData = pUserData;
}

const PropertyDef** SceneViewModel::GetProperties()
{
	return nullptr;
}

void SceneViewModel::AddObject(WorldObject* pObject)
{
	m_pScene->AddObject(pObject);
	m_addedObjectCallback(pObject, m_pUserData);
}
