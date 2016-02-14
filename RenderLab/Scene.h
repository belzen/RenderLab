#pragma once

#include <vector>
#include "render/RdrContext.h"
#include "render/Light.h"

class Camera;
class Renderer;
class WorldObject;
struct RdrContext;

class Scene
{
public:
	Scene();

	void Load(RdrContext* pContext, const char* filename);

	void Update(float dt);
	void QueueDraw(Renderer* pRenderer);

private:
	std::vector<WorldObject*> m_objects;
	std::vector<Light> m_lights;
	LightList m_lightList;
};
