#pragma once

#include "render/RdrContext.h"
#include "render/Terrain.h"
#include "render/RdrLighting.h"
#include <vector>
#include "AssetLib/SceneAsset.h"
#include "AssetLib/AssetLibrary.h"

class Camera;
class Renderer;
class Entity;
class RdrContext;

typedef std::vector<Entity*> EntityList;

class Scene : public IAssetReloadListener<AssetLib::Scene>
{
public:
	Scene();

	void Load(const char* sceneName);

	void Update();

	const Terrain& GetTerrain() const;
	Terrain& GetTerrain();

	const EntityList& GetEntities() const;
	EntityList& GetEntities();

	void AddEntity(Entity* pEntity);
	void RemoveEntity(Entity* pEntity);

	RdrResourceHandle GetEnvironmentMapTexArray() const;
	void InvalidateEnvironmentLights();

	const char* GetName() const;

	const Vec3& GetCameraSpawnPosition() const;
	const Rotation& GetCameraSpawnRotation() const;

private:
	void OnAssetReloaded(const AssetLib::Scene* pSceneAsset);
	void CaptureEnvironmentLight(Light* pLight);

	void Cleanup();

private:
	EntityList m_entities;
	Terrain m_terrain;

	Vec3 m_cameraSpawnPosition;
	Rotation m_cameraSpawnRotation;

	Light* m_apActiveEnvironmentLights[MAX_ENVIRONMENT_MAPS];
	RdrResourceHandle m_hEnvironmentMapTexArray;
	RdrResourceHandle m_hEnvironmentMapDepthBuffer;
	RdrDepthStencilViewHandle m_hEnvironmentMapDepthView;
	uint m_environmentMapSize;

	CachedString m_sceneName;
	bool m_reloadPending;
};

//////////////////////////////////////////////////////////////////////////

inline const Terrain& Scene::GetTerrain() const
{
	return m_terrain;
}

inline Terrain& Scene::GetTerrain()
{
	return m_terrain;
}

inline const EntityList& Scene::GetEntities() const
{
	return m_entities;
}

inline EntityList& Scene::GetEntities()
{
	return m_entities;
}

inline RdrResourceHandle Scene::GetEnvironmentMapTexArray() const
{
	return m_hEnvironmentMapTexArray;
}