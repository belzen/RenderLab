#pragma once

#include "render/RdrPostProcessEffects.h"
#include "render/RdrContext.h"
#include "render/Light.h"
#include "render/Sky.h"
#include "render/Terrain.h"
#include <vector>

class Camera;
class Renderer;
class WorldObject;
class RdrContext;
class RdrPostProcessEffects;

typedef std::vector<WorldObject*> WorldObjectList;

class Scene
{
public:
	Scene();

	void Reload();
	void Load(const char* sceneName);

	void Update(float dt);

	const Sky& GetSky() const;
	Sky& GetSky();

	const Terrain& GetTerrain() const;
	Terrain& GetTerrain();

	const WorldObjectList& GetWorldObjects() const;
	WorldObjectList& GetWorldObjects();

	void AddObject(WorldObject* pObject);

	LightList& GetLightList();

	const RdrPostProcessEffects* GetPostProcEffects() const;
	RdrPostProcessEffects* GetPostProcEffects();

	const char* GetName() const;

private:
	void Cleanup();

	WorldObjectList m_objects;
	LightList m_lights;
	Sky m_sky;
	Terrain m_terrain;
	RdrPostProcessEffects m_postProcEffects;
	FileWatcher::ListenerID m_reloadListenerId;
	char m_sceneName[AssetLib::AssetDef::kMaxNameLen];
	bool m_reloadPending;
};

inline const Sky& Scene::GetSky() const
{
	return m_sky;
}

inline Sky& Scene::GetSky()
{
	return m_sky;
}

inline const Terrain& Scene::GetTerrain() const
{
	return m_terrain;
}

inline Terrain& Scene::GetTerrain()
{
	return m_terrain;
}

inline const WorldObjectList& Scene::GetWorldObjects() const
{
	return m_objects;
}

inline WorldObjectList& Scene::GetWorldObjects()
{
	return m_objects;
}

inline LightList& Scene::GetLightList()
{ 
	return m_lights; 
}

inline const RdrPostProcessEffects* Scene::GetPostProcEffects() const
{
	return &m_postProcEffects;
}

inline RdrPostProcessEffects* Scene::GetPostProcEffects()
{
	return &m_postProcEffects;
}

inline const char* Scene::GetName() const
{
	return m_sceneName;
}
