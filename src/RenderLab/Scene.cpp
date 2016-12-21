#include "Precompiled.h"
#include "Scene.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "WorldObject.h"
#include "components/ModelInstance.h"
#include "render/Font.h"
#include "AssetLib/SceneAsset.h"
#include "AssetLib/AssetLibrary.h"

namespace
{
	uint s_stressTestLights = 0;
	void spawnLightStressTest(LightList& rLightList)
	{
		for (float x = -150.f; x < 150.f; x += 8.f)
		{
			for (float y = -150.f; y < 150.f; y += 8.f)
			{
				rLightList.AddPointLight(Vec3(x, 5.f, y), Vec3(800.f, 0.f, 0.f), 16.f);
			}
		}
	}
}

Scene::Scene()
	: m_reloadPending(false)
{
}

void Scene::Cleanup()
{
	AssetLibrary<AssetLib::Scene>::RemoveReloadListener(this);

	uint numObjects = (uint)m_objects.size();
	for (uint i = 0; i < numObjects; ++i)
	{
		m_objects[i]->Release();
	}
	m_objects.clear();

	m_lights.Cleanup();
	m_sky.Cleanup();
	m_sceneName = nullptr;
}

void Scene::OnAssetReloaded(const AssetLib::Scene* pSceneAsset)
{
	if (m_sceneName == pSceneAsset->assetName)
	{
		m_reloadPending = true;
	}
}

void Scene::Load(const char* sceneName)
{
	assert(!m_sceneName.getString());
	m_sceneName = sceneName;

	AssetLib::Scene* pSceneData = AssetLibrary<AssetLib::Scene>::LoadAsset(m_sceneName);
	AssetLibrary<AssetLib::Scene>::AddReloadListener(this);
	if (!pSceneData)
	{
		assert(false);
		return;
	}

	// Camera
	m_cameraSpawnPosition = pSceneData->camPosition;
	m_cameraSpawnPitchYawRoll = pSceneData->camPitchYawRoll;

	// Sky
	m_sky.Load(pSceneData->skyName);

	// Post-processing effects
	{
		AssetLib::PostProcessEffects* pEffects = AssetLibrary<AssetLib::PostProcessEffects>::LoadAsset(pSceneData->postProcessingEffectsName);
		if (!pEffects)
		{
			assert(false);
			return;
		}

		m_postProcEffects.Init(pEffects);
	}

	// Objects
	for (const AssetLib::Object& rObjectData : pSceneData->objects)
	{
		WorldObject* pObject = WorldObject::Create(rObjectData.name, rObjectData.position, rObjectData.orientation, rObjectData.scale);
		pObject->SetModel(ModelInstance::Create(rObjectData.modelName, rObjectData.materialSwaps, rObjectData.numMaterialSwaps));

		RigidBody* pRigidBody = nullptr;
		switch (rObjectData.physics.shape)
		{
		case AssetLib::ShapeType::Box:
			pRigidBody = RigidBody::CreateBox(rObjectData.physics.halfSize, rObjectData.physics.density, rObjectData.physics.offset);
			break;
		case AssetLib::ShapeType::Sphere:
			pRigidBody = RigidBody::CreateSphere(rObjectData.physics.halfSize.x, rObjectData.physics.density, rObjectData.physics.offset);
			break;
		}
		pObject->SetRigidBody(pRigidBody);

		m_objects.push_back(pObject);
	}

	if (pSceneData->terrain.enabled)
	{
		m_terrain.Init(pSceneData->terrain);
	}

	// Lights
	m_lights.Init(this);
	m_lights.SetGlobalEnvironmentLight(pSceneData->globalEnvironmentLightPosition);
	for (const AssetLib::Light& rLightData : pSceneData->lights)
	{
		switch (rLightData.type)
		{
		case LightType::Directional:
			m_lights.AddDirectionalLight(rLightData.direction, rLightData.color);
			break;
		case LightType::Point:
			m_lights.AddPointLight(rLightData.position, rLightData.color, rLightData.radius);
			break;
		case LightType::Spot:
			m_lights.AddSpotLight(rLightData.position, rLightData.direction, rLightData.color, rLightData.radius, rLightData.innerConeAngle, rLightData.outerConeAngle);
			break;
		case LightType::Environment:
			m_lights.AddEnvironmentLight(rLightData.position);
			break;
		}
	}

	if (s_stressTestLights)
	{
		spawnLightStressTest(m_lights);
	}

	// TODO: quad/oct tree for scene
}

void Scene::Update()
{
	if (m_reloadPending)
	{
		Cleanup();
		Load(m_sceneName.getString());
		m_reloadPending = false;
	}

	m_sky.Update();

	for (WorldObject* pObj : m_objects)
	{
		pObj->Update();
	}
}

void Scene::AddObject(WorldObject* pObject)
{
	m_objects.push_back(pObject);
}

const char* Scene::GetName() const
{
	return m_sceneName.getString();
}

const Vec3& Scene::GetCameraSpawnPosition() const
{
	return m_cameraSpawnPosition;
}

const Vec3& Scene::GetCameraSpawnPitchYawRoll() const
{
	return m_cameraSpawnPitchYawRoll;
}
