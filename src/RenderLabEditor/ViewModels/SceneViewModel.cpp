#include "Precompiled.h"
#include "SceneViewModel.h"
#include "Scene.h"
#include "Entity.h"
#include "PropertyTables.h"
#include "Widgets/TreeView.h"

const char* SceneViewModel::GetTypeName()
{
	return "Scene";
}

const PropertyDef** SceneViewModel::GetProperties()
{
	return nullptr;
}

void SceneViewModel::SetListener(ISceneListener* pListener)
{
	m_pListener = pListener;
}

void SceneViewModel::AddEntity(Entity* pEntity)
{
	Scene::AddEntity(pEntity);
	if (m_pListener)
	{
		m_pListener->OnEntityAddedToScene(pEntity);
	}
}

void SceneViewModel::RemoveObject(Entity* pEntity)
{
	if (!pEntity)
		return;

	Scene::RemoveEntity(pEntity);
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

	for (Entity* pEntity : Scene::GetEntities())
	{
		pTreeView->AddItem(pEntity->GetName(), pEntity);
	}
}
