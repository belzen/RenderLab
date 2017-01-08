#pragma once

#include "IViewModel.h"

class Scene;
class WorldObject;
class TreeView;

class ISceneListener
{
public:
	virtual void OnSceneObjectAdded(WorldObject* pObject) = 0;
	virtual void OnSceneObjectRemoved(WorldObject* pObject) = 0;
	virtual void OnSceneSelectionChanged(WorldObject* pObject) = 0;
};

class SceneViewModel : public IViewModel
{
public:
	void Init(Scene* pScene);

	const PropertyDef** GetProperties();

	Scene* GetScene() const;

	void AddObject(WorldObject* pObject);
	void RemoveObject(WorldObject* pObject);

	void SetSelected(WorldObject* pObject);
	WorldObject* GetSelected();

	void PopulateTreeView(TreeView* pTreeView);
	
	void SetListener(ISceneListener* pListener);

private:
	Scene* m_pScene;
	ISceneListener* m_pListener;
	WorldObject* m_pSelectedObject;
};