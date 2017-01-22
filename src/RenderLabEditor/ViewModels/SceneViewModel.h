#pragma once

#include "IViewModel.h"

class Entity;
class TreeView;

class ISceneListener
{
public:
	virtual void OnEntityAddedToScene(Entity* pObject) = 0;
	virtual void OnEntityRemovedFromScene(Entity* pObject) = 0;
	virtual void OnSceneSelectionChanged(Entity* pObject) = 0;
};

class SceneViewModel : public IViewModel
{
public:
	const char* GetTypeName();
	const PropertyDef** GetProperties();

	void AddEntity(Entity* pObject);
	void RemoveObject(Entity* pObject);

	void SetSelected(Entity* pObject);
	Entity* GetSelected();

	void PopulateTreeView(TreeView* pTreeView);
	
	void SetListener(ISceneListener* pListener);

private:
	ISceneListener* m_pListener;
	Entity* m_pSelectedEntity;
};