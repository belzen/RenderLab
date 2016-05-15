#pragma once

#include "AssetLib/PostProcessEffectsAsset.h"
#include "render/RdrContext.h"
#include "render/Light.h"
#include "render/Sky.h"

class Camera;
class Renderer;
class WorldObject;
class RdrContext;

class Scene
{
public:
	Scene();

	void Reload();
	void Load(const char* sceneName);

	void Update(float dt);
	void QueueShadowMaps(Renderer& rRenderer);
	void QueueDraw(Renderer& rRenderer) const;

	const LightList* GetLightList() const;

	const AssetLib::PostProcessEffects& GetPostProcEffects() const;

	Camera& GetMainCamera();
	const Camera& GetMainCamera() const;

	const char* GetName() const;

private:
	void Cleanup();

	std::vector<WorldObject*> m_objects;
	LightList m_lights;
	Camera m_mainCamera;
	Sky m_sky;
	AssetLib::PostProcessEffects m_postProcEffects;
	FileWatcher::ListenerID m_reloadListenerId;
	char m_sceneName[AssetLib::AssetDef::kMaxNameLen];
	bool m_reloadPending;
};

inline const LightList* Scene::GetLightList() const
{ 
	return &m_lights; 
}

inline const AssetLib::PostProcessEffects& Scene::GetPostProcEffects() const
{
	return m_postProcEffects;
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
