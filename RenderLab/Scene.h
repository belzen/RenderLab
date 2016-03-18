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
	void QueueShadowMaps(Renderer& rRenderer, const Camera* pCamera);
	void QueueDraw(Renderer& rRenderer) const;

	const LightList* GetLightList() const { return &m_lights; }

private:
	std::vector<WorldObject*> m_objects;
	LightList m_lights;
};
