#pragma once

#include "render/RdrPostProcessEffects.h"
#include "render/RdrContext.h"
#include "render/Sky.h"
#include "render/Terrain.h"
#include "render/RdrLighting.h"
#include <vector>
#include "AssetLib/SceneAsset.h"
#include "AssetLib/AssetLibrary.h"

class Camera;
class Renderer;
class WorldObject;
class RdrContext;
class RdrPostProcessEffects;

typedef std::vector<WorldObject*> WorldObjectList;

class Scene : public IAssetReloadListener<AssetLib::Scene>
{
public:
	Scene();

	void Load(const char* sceneName);

	void Update();

	const Sky& GetSky() const;
	Sky& GetSky();

	const Terrain& GetTerrain() const;
	Terrain& GetTerrain();

	const WorldObjectList& GetWorldObjects() const;
	WorldObjectList& GetWorldObjects();

	void AddObject(WorldObject* pObject);
	void RemoveObject(WorldObject* pObject);

	const RdrPostProcessEffects* GetPostProcEffects() const;
	RdrPostProcessEffects* GetPostProcEffects();

	RdrResourceHandle GetEnvironmentMapTexArray() const;
	void InvalidateEnvironmentLights();

	const char* GetName() const;

	const Vec3& GetCameraSpawnPosition() const;
	const Vec3& GetCameraSpawnPitchYawRoll() const;

private:
	void OnAssetReloaded(const AssetLib::Scene* pSceneAsset);
	void CaptureEnvironmentLight(Light* pLight);

	void Cleanup();

private:
	WorldObjectList m_objects;
	Sky m_sky;
	Terrain m_terrain;
	RdrPostProcessEffects m_postProcEffects;

	Vec3 m_cameraSpawnPosition;
	Vec3 m_cameraSpawnPitchYawRoll;

	Light* m_apActiveEnvironmentLights[MAX_ENVIRONMENT_MAPS];
	RdrResourceHandle m_hEnvironmentMapTexArray;
	RdrResourceHandle m_hEnvironmentMapDepthBuffer;
	RdrDepthStencilViewHandle m_hEnvironmentMapDepthView;
	uint m_environmentMapSize;

	CachedString m_sceneName;
	bool m_reloadPending;
};

//////////////////////////////////////////////////////////////////////////

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

inline const RdrPostProcessEffects* Scene::GetPostProcEffects() const
{
	return &m_postProcEffects;
}

inline RdrPostProcessEffects* Scene::GetPostProcEffects()
{
	return &m_postProcEffects;
}

inline RdrResourceHandle Scene::GetEnvironmentMapTexArray() const
{
	return m_hEnvironmentMapTexArray;
}