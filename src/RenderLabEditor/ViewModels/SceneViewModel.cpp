#include "Precompiled.h"
#include "SceneViewModel.h"
#include "Scene.h"
#include "Entity.h"
#include "PropertyTables.h"
#include "Widgets/TreeView.h"

void SceneViewModel::Init(Scene* pScene)
{
	m_pScene = pScene;
}

const char* SceneViewModel::GetTypeName()
{
	return "Scene";
}

const PropertyDef** SceneViewModel::GetProperties()
{
	return nullptr;
}

Scene* SceneViewModel::GetScene() const
{
	return m_pScene;
}

void SceneViewModel::SetListener(ISceneListener* pListener)
{
	m_pListener = pListener;
}

void SceneViewModel::AddEntity(Entity* pEntity)
{
	m_pScene->AddEntity(pEntity);
	if (m_pListener)
	{
		m_pListener->OnEntityAddedToScene(pEntity);
	}
}

void SceneViewModel::RemoveObject(Entity* pEntity)
{
	if (!pEntity)
		return;

	m_pScene->RemoveEntity(pEntity);
	if (m_pListener)
	{
		m_pListener->OnEntityRemovedFromScene(pEntity);
	}
}

void SceneViewModel::SetSelected(Entity* pEntity)
{
	m_pSelectedEntity = pEntity;
	if (m_pListener)
	{
		m_pListener->OnSceneSelectionChanged(pEntity);
	}
}

Entity* SceneViewModel::GetSelected()
{
	return m_pSelectedEntity;
}

void SceneViewModel::PopulateTreeView(TreeView* pTreeView)
{
	pTreeView->Clear();

	EntityList& rEntities = m_pScene->GetEntities();
	for (Entity* pEntity : rEntities)
	{
		pTreeView->AddItem(pEntity->GetName(), pEntity);
	}
}
