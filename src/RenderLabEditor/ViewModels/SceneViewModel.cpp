#include "Precompiled.h"
#include "SceneViewModel.h"
#include "Scene.h"
#include "WorldObject.h"
#include "PropertyTables.h"
#include "AssetLib/SkyAsset.h"
#include "Widgets/TreeView.h"

void SceneViewModel::Init(Scene* pScene)
{
	m_pScene = pScene;
}

const PropertyDef** SceneViewModel::GetProperties()
{
	return nullptr;
}

Scene* SceneViewModel::GetScene() const
{
	return m_pScene;
}

void SceneViewModel::SetObjectAddedCallback(ObjectAddedFunc objectAddedCallback, void* pUserData)
{
	m_objectAddedCallback = objectAddedCallback;
	m_pObjectAddedUserData = pUserData;
}

void SceneViewModel::SetObjectRemovedCallback(ObjectRemovedFunc objectRemovedCallback, void* pUserData)
{
	m_objectRemovedCallback = objectRemovedCallback;
	m_pObjectRemovedUserData = pUserData;
}

void SceneViewModel::AddObject(WorldObject* pObject)
{
	m_pScene->AddObject(pObject);
	if (m_objectAddedCallback)
	{
		m_objectAddedCallback(pObject, m_pObjectAddedUserData);
	}
}

void SceneViewModel::RemoveObject(WorldObject* pObject)
{
	m_pScene->RemoveObject(pObject);
	if (m_objectRemovedCallback)
	{
		m_objectRemovedCallback(pObject, m_pObjectRemovedUserData);
	}
}

void SceneViewModel::PopulateTreeView(TreeView* pTreeView)
{
	pTreeView->Clear();

	AssetLib::Sky* pSky = AssetLibrary<AssetLib::Sky>::LoadAsset(m_pScene->GetSky().GetSourceAsset()->assetName);
	pTreeView->AddItem("Sky", pSky);

	AssetLib::PostProcessEffects* pEffects = AssetLibrary<AssetLib::PostProcessEffects>::LoadAsset(m_pScene->GetPostProcEffects()->GetEffectsAsset()->assetName);
	pTreeView->AddItem("Post-Processing Effects", pEffects);

	WorldObjectList& sceneObjects = m_pScene->GetWorldObjects();
	for (WorldObject* pObject : sceneObjects)
	{
		pTreeView->AddItem(pObject->GetName(), pObject);
	}
}
