#include "Precompiled.h"
#include "Scene.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "render/Font.h"
#include "render/RdrOffscreenTasks.h"
#include "Entity.h"
#include "AssetLib/SceneAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "Game.h"

namespace
{
	struct
	{
		DefaultComponentAllocator m_componentAllocator;
		EntityList m_entities;
		Terrain m_terrain;

		Vec3 m_cameraSpawnPosition;
		Rotation m_cameraSpawnRotation;

		Light* m_apActiveEnvironmentLights[MAX_ENVIRONMENT_MAPS];
		RdrResourceHandle m_hEnvironmentMapTexArray;
		RdrResourceHandle m_hEnvironmentMapDepthBuffer;
		RdrDepthStencilViewHandle m_hEnvironmentMapDepthView;
		uint m_environmentMapSize;

		CachedString m_name;
	} s_scene;

	void captureEnvironmentLight(Light* pLight)
	{
		int index = pLight->GetEnvironmentTextureIndex();
		if (index < 0)
		{
			// Find an empty environment light slot
			for (int i = 0; i < ARRAY_SIZE(s_scene.m_apActiveEnvironmentLights); ++i)
			{
				if (!s_scene.m_apActiveEnvironmentLights[i])
				{
					s_scene.m_apActiveEnvironmentLights[i] = pLight;
					pLight->SetEnvironmentTextureIndex(i);
					index = i;
					break;
				}
			}

			// Bail if there were no open slots.
			if (index < 0)
				return;
		}

		assert(s_scene.m_apActiveEnvironmentLights[index] == pLight);
		Rect viewport(0.f, 0.f, (float)s_scene.m_environmentMapSize, (float)s_scene.m_environmentMapSize);
		RdrOffscreenTasks::QueueSpecularProbeCapture(pLight->GetEntity()->GetPosition(), viewport,
			s_scene.m_hEnvironmentMapTexArray, index, s_scene.m_hEnvironmentMapDepthView);
	}
}

void Scene::Cleanup()
{
	for (Entity* pEntity : s_scene.m_entities)
	{
		pEntity->Release();
	}
	s_scene.m_entities.clear();

	s_scene.m_name = nullptr;
}

void Scene::InvalidateEnvironmentLights()
{
	for (int i = 0; i < ARRAY_SIZE(s_scene.m_apActiveEnvironmentLights); ++i)
	{
		if (s_scene.m_apActiveEnvironmentLights[i])
		{
			captureEnvironmentLight(s_scene.m_apActiveEnvironmentLights[i]);
		}
	}
}

void Scene::Load(const char* sceneName)
{
	if (s_scene.m_name.getString())
	{
		Cleanup();
	}

	s_scene.m_name = sceneName;

	AssetLib::Scene* pSceneData = AssetLibrary<AssetLib::Scene>::LoadAsset(s_scene.m_name);
	if (!pSceneData)
	{
		assert(false);
		return;
	}

	memset(s_scene.m_apActiveEnvironmentLights, 0, sizeof(s_scene.m_apActiveEnvironmentLights));
	if (!s_scene.m_hEnvironmentMapTexArray || s_scene.m_environmentMapSize != pSceneData->environmentMapTexSize)
	{
		RdrResourceCommandList& rResCommands = g_pRenderer->GetPreFrameCommandList();
		if (s_scene.m_hEnvironmentMapTexArray)
		{
			rResCommands.ReleaseResource(s_scene.m_hEnvironmentMapTexArray);
			s_scene.m_hEnvironmentMapTexArray = 0;
		}

		s_scene.m_environmentMapSize = pSceneData->environmentMapTexSize;
		s_scene.m_hEnvironmentMapTexArray = rResCommands.CreateTextureCubeArray(s_scene.m_environmentMapSize, s_scene.m_environmentMapSize, MAX_ENVIRONMENT_MAPS, RdrResourceFormat::R16G16B16A16_FLOAT);
		s_scene.m_hEnvironmentMapDepthBuffer = rResCommands.CreateTexture2D(s_scene.m_environmentMapSize, s_scene.m_environmentMapSize, RdrResourceFormat::D24_UNORM_S8_UINT, RdrResourceUsage::Default, nullptr);
		s_scene.m_hEnvironmentMapDepthView = rResCommands.CreateDepthStencilView(s_scene.m_hEnvironmentMapDepthBuffer);
	}

	// Camera
	s_scene.m_cameraSpawnPosition = pSceneData->camPosition;
	s_scene.m_cameraSpawnRotation = pSceneData->camRotation;

	// Objects/Entities
	for (const AssetLib::Object& rObjectData : pSceneData->objects)
	{
		Entity* pEntity = Entity::Create(rObjectData.name, rObjectData.position, rObjectData.rotation, rObjectData.scale);

		// Model
		if (rObjectData.modelName)
		{
			pEntity->AttachRenderable(ModelInstance::Create(&s_scene.m_componentAllocator, rObjectData.modelName, rObjectData.materialSwaps, rObjectData.numMaterialSwaps));
		}

		// Physics
		RigidBody* pRigidBody = nullptr;
		switch (rObjectData.physics.shape)
		{
		case AssetLib::ShapeType::Box:
			pRigidBody = RigidBody::CreateBox(&s_scene.m_componentAllocator, rObjectData.physics.halfSize, rObjectData.physics.density, rObjectData.physics.offset);
			break;
		case AssetLib::ShapeType::Sphere:
			pRigidBody = RigidBody::CreateSphere(&s_scene.m_componentAllocator, rObjectData.physics.halfSize.x, rObjectData.physics.density, rObjectData.physics.offset);
			break;
		}
		pEntity->AttachRigidBody(pRigidBody);

		// Lighting
		switch (rObjectData.light.type)
		{
		case LightType::Directional:
			pEntity->AttachLight(Light::CreateDirectional(&s_scene.m_componentAllocator, rObjectData.light.color, rObjectData.light.intensity, rObjectData.light.pssmLambda));
			break;
		case LightType::Point:
			pEntity->AttachLight(Light::CreatePoint(&s_scene.m_componentAllocator, rObjectData.light.color, rObjectData.light.intensity, rObjectData.light.radius));
			break;
		case LightType::Spot:
			pEntity->AttachLight(Light::CreateSpot(&s_scene.m_componentAllocator, rObjectData.light.color, rObjectData.light.intensity, rObjectData.light.radius, rObjectData.light.innerConeAngle, rObjectData.light.outerConeAngle));
			break;
		case LightType::Environment:
			{
				Light* pLight = Light::CreateEnvironment(&s_scene.m_componentAllocator, rObjectData.light.bIsGlobalEnvironmentLight);
				pEntity->AttachLight(pLight);
				captureEnvironmentLight(pLight);
			}
			break;
		}

		// Volume component
		switch (rObjectData.volume.volumeType)
		{
		case AssetLib::VolumeType::kPostProcess:
			pEntity->AttachVolume(PostProcessVolume::Create(&s_scene.m_componentAllocator, rObjectData.volume));
			break;
		case AssetLib::VolumeType::kSky:
			pEntity->AttachVolume(SkyVolume::Create(&s_scene.m_componentAllocator, rObjectData.volume));
			break;
		}

		s_scene.m_entities.push_back(pEntity);
	}

	if (pSceneData->terrain.enabled)
	{
		s_scene.m_terrain.Init(pSceneData->terrain);
	}

	// TODO: quad/oct tree for scene
}

void Scene::Update()
{
	if (Game::IsActive())
	{
		Physics::Update();

		for (RigidBody& rRigidBody : s_scene.m_componentAllocator.GetRigidBodyFreeList())
		{
			rRigidBody.UpdatePostSimulation();
		}
	}
	else
	{
		for (RigidBody& rRigidBody : s_scene.m_componentAllocator.GetRigidBodyFreeList())
		{
			rRigidBody.UpdateNoSimulation();
		}
	}
}

void Scene::AddEntity(Entity* pEntity)
{
	s_scene.m_entities.push_back(pEntity);
}

void Scene::RemoveEntity(Entity* pEntity)
{
	auto iter = std::find(s_scene.m_entities.begin(), s_scene.m_entities.end(), pEntity);
	if (iter != s_scene.m_entities.end())
	{
		s_scene.m_entities.erase(iter);
	}
}

const char* Scene::GetName()
{
	return s_scene.m_name.getString();
}

const Vec3& Scene::GetCameraSpawnPosition()
{
	return s_scene.m_cameraSpawnPosition;
}

const Rotation& Scene::GetCameraSpawnRotation()
{
	return s_scene.m_cameraSpawnRotation;
}

DefaultComponentAllocator* Scene::GetComponentAllocator()
{
	return &s_scene.m_componentAllocator;
}

Terrain& Scene::GetTerrain()
{
	return s_scene.m_terrain;
}

EntityList& Scene::GetEntities()
{
	return s_scene.m_entities;
}

RdrResourceHandle Scene::GetEnvironmentMapTexArray()
{
	return s_scene.m_hEnvironmentMapTexArray;
}