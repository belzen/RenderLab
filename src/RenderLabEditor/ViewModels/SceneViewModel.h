#pragma once

#include "IViewModel.h"

class Scene;
class WorldObject;
class TreeView;

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
	
	// Event handlers
	typedef void (*ObjectAddedFunc)(WorldObject* pObject, void* pUserData);
	void SetObjectAddedCallback(ObjectAddedFunc objectAddedCallback, void* pUserData);

	typedef void (*ObjectRemovedFunc)(WorldObject* pObject, void* pUserData);
	void SetObjectRemovedCallback(ObjectRemovedFunc objectRemovedCallback, void* pUserData);

	typedef void (*SelectionChangedFunc)(WorldObject* pObject, void* pUserData);
	void SetSelectionChangedCallback(SelectionChangedFunc callback, void* pUserData);

private:
	Scene* m_pScene;
	WorldObject* m_pSelectedObject;

	ObjectAddedFunc m_objectAddedCallback;
	void* m_pObjectAddedUserData;
	ObjectRemovedFunc m_objectRemovedCallback;
	void* m_pObjectRemovedUserData;
	SelectionChangedFunc m_selectionChangedCallback;
	void* m_pSelectionChangedUserData;
};