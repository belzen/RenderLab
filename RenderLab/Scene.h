#pragma once

#include <vector>
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

	void Load(Renderer& rRenderer, const char* filename);

	void Update(float dt);
	void QueueShadowMaps(Renderer& rRenderer, const Camera& rCamera);
	void QueueDraw(Renderer& rRenderer) const;

	const LightList* GetLightList() const;

	Camera& GetMainCamera();
	const Camera& GetMainCamera() const;

private:
	std::vector<WorldObject*> m_objects;
	LightList m_lights;
	Camera m_mainCamera;
	Sky m_sky;
};

inline const LightList* Scene::GetLightList() const
{ 
	return &m_lights; 
}

inline Camera& Scene::GetMainCamera()
{ 
	return m_mainCamera; 
}

inline const Camera& Scene::GetMainCamera() const
{ 
	return m_mainCamera; 
}
