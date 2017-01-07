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

	void PopulateTreeView(TreeView* pTreeView);
	
	// Event handlers
	typedef void (*ObjectAddedFunc)(WorldObject* pObject, void* pUserData);
	void SetObjectAddedCallback(ObjectAddedFunc objectAddedCallback, void* pUserData);

	typedef void (*ObjectRemovedFunc)(WorldObject* pObject, void* pUserData);
	void SetObjectRemovedCallback(ObjectRemovedFunc objectRemovedCallback, void* pUserData);
private:
	Scene* m_pScene;

	int m_stateId;

	ObjectAddedFunc m_objectAddedCallback;
	void* m_pObjectAddedUserData;
	ObjectRemovedFunc m_objectRemovedCallback;
	void* m_pObjectRemovedUserData;
};