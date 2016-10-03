#pragma once

#include "IViewModel.h"

class Scene;
class WorldObject;

class SceneViewModel : public IViewModel
{
public:
	typedef void (*AddedObjectFunc)(WorldObject* pObject, void* pUserData);

	void Init(Scene* pScene, AddedObjectFunc addedObjectCallback, void* pUserData);

	const PropertyDef** GetProperties();

	void AddObject(WorldObject* pObject);

private:
	Scene* m_pScene;

	AddedObjectFunc m_addedObjectCallback;
	void* m_pUserData;
};