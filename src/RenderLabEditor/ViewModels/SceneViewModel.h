#pragma once

#include "IViewModel.h"

class Scene;
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
	void Init(Scene* pScene);

	const char* GetTypeName();
	const PropertyDef** GetProperties();

	Scene* GetScene() const;

	void AddEntity(Entity* pObject);
	void RemoveObject(Entity* pObject);

	void SetSelected(Entity* pObject);
	Entity* GetSelected();

	void PopulateTreeView(TreeView* pTreeView);
	
	void SetListener(ISceneListener* pListener);

private:
	Scene* m_pScene;
	ISceneListener* m_pListener;
	Entity* m_pSelectedEntity;
};