#pragma once

#include <vector>
#include "render/RdrContext.h"
#include "render/Light.h"

class Camera;
class Renderer;
class WorldObject;
class RdrContext;

class Scene
{
public:
	Scene();

	void Load(RdrContext* pContext, const char* filename);

	void Update(float dt);
	void QueueShadowMaps(Renderer& rRenderer, const Camera& rCamera);
	void QueueDraw(Renderer& rRenderer) const;

	const LightList* GetLightList() const { return &m_lights; }

	Camera& GetMainCamera() { return m_mainCamera; }
	const Camera& GetMainCamera() const { return m_mainCamera; }

private:
	std::vector<WorldObject*> m_objects;
	LightList m_lights;
	Camera m_mainCamera;
};
