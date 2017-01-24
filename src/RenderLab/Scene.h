#pragma once

#include "render/RdrContext.h"
#include "render/Terrain.h"
#include "render/RdrLighting.h"
#include "AssetLib/SceneAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "components/ComponentAllocator.h"
#include "Physics.h"
#include <vector>

class Camera;
class Renderer;
class Entity;
class RdrContext;

typedef std::vector<Entity*> EntityList;

namespace Scene
{
	void Load(const char* sceneName);
	void Cleanup();

	void Update();
	void QueueDraw(RdrAction* pAction);

	Terrain& GetTerrain();

	EntityList& GetEntities();

	void AddEntity(Entity* pEntity);
	void RemoveEntity(Entity* pEntity);

	RdrResourceHandle GetEnvironmentMapTexArray();
	void InvalidateEnvironmentLights();

	const char* GetName();

	const Vec3& GetCameraSpawnPosition();
	const Rotation& GetCameraSpawnRotation();

	DefaultComponentAllocator* GetComponentAllocator();
}
