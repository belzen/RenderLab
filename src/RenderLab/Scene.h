#pragma once

#include "render/RdrPostProcessEffects.h"
#include "render/RdrContext.h"
#include "render/Light.h"
#include "render/Sky.h"

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

	void PrepareDraw();

	void QueueShadowMaps(Renderer& rRenderer);

	const Sky& GetSky() const;

	const WorldObjectList& GetWorldObjects() const;

	const LightList* GetLightList() const;

	const RdrPostProcessEffects* GetPostProcEffects() const;

	Camera& GetMainCamera();
	const Camera& GetMainCamera() const;

	const char* GetName() const;

private:
	void Cleanup();

	WorldObjectList m_objects;
	LightList m_lights;
	Camera m_mainCamera;
	Sky m_sky;
	RdrPostProcessEffects m_postProcEffects;
	FileWatcher::ListenerID m_reloadListenerId;
	char m_sceneName[AssetLib::AssetDef::kMaxNameLen];
	bool m_reloadPending;
};

inline const Sky& Scene::GetSky() const
{
	return m_sky;
}

inline const WorldObjectList& Scene::GetWorldObjects() const
{
	return m_objects;
}

inline const LightList* Scene::GetLightList() const
{ 
	return &m_lights; 
}

inline const RdrPostProcessEffects* Scene::GetPostProcEffects() const
{
	return &m_postProcEffects;
}

inline Camera& Scene::GetMainCamera()
{ 
	return m_mainCamera; 
}

inline const Camera& Scene::GetMainCamera() const
{ 
	return m_mainCamera; 
}

inline const char* Scene::GetName() const
{
	return m_sceneName;
}
