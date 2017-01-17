#include "Precompiled.h"
#include "Scene.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "render/Font.h"
#include "render/RdrOffscreenTasks.h"
#include "Entity.h"
#include "components/ModelInstance.h"
#include "components/Light.h"
#include "components/RigidBody.h"
#include "components/PostProcessVolume.h"
#include "components/SkyVolume.h"
#include "AssetLib/SceneAsset.h"
#include "AssetLib/AssetLibrary.h"

Scene::Scene()
	: m_environmentMapSize(128)
	, m_reloadPending(false)
{
}

void Scene::Cleanup()
{
	AssetLibrary<AssetLib::Scene>::RemoveReloadListener(this);

	for (Entity* pEntity : m_entities)
	{
		pEntity->Release();
	}
	m_entities.clear();

	m_sceneName = nullptr;
}

void Scene::OnAssetReloaded(const AssetLib::Scene* pSceneAsset)
{
	if (m_sceneName == pSceneAsset->assetName)
	{
		m_reloadPending = true;
	}
}

void Scene::InvalidateEnvironmentLights()
{
	for (int i = 0; i < ARRAY_SIZE(m_apActiveEnvironmentLights); ++i)
	{
		if (m_apActiveEnvironmentLights[i])
		{
			CaptureEnvironmentLight(m_apActiveEnvironmentLights[i]);
		}
	}
}

void Scene::CaptureEnvironmentLight(Light* pLight)
{
	int index = pLight->GetEnvironmentTextureIndex();
	if (index < 0)
	{
		// Find an empty environment light slot
		for (int i = 0; i < ARRAY_SIZE(m_apActiveEnvironmentLights); ++i)
		{
			if (!m_apActiveEnvironmentLights[i])
			{
				m_apActiveEnvironmentLights[i] = pLight;
				pLight->SetEnvironmentTextureIndex(i);
				index = i;
				break;
			}
		}

		// Bail if there were no open slots.
		if (index < 0)
			return;
	}

	assert(m_apActiveEnvironmentLights[index] == pLight);
	Rect viewport(0.f, 0.f, (float)m_environmentMapSize, (float)m_environmentMapSize);
	RdrOffscreenTasks::QueueSpecularProbeCapture(pLight->GetEntity()->GetPosition(), this, viewport,
		m_hEnvironmentMapTexArray, index, m_hEnvironmentMapDepthView);
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

	memset(m_apActiveEnvironmentLights, 0, sizeof(m_apActiveEnvironmentLights));
	if (!m_hEnvironmentMapTexArray)
	{
		RdrResourceCommandList& rResCommands = g_pRenderer->GetPreFrameCommandList();
		m_hEnvironmentMapTexArray = rResCommands.CreateTextureCubeArray(m_environmentMapSize, m_environmentMapSize, MAX_ENVIRONMENT_MAPS, RdrResourceFormat::R16G16B16A16_FLOAT);
		m_hEnvironmentMapDepthBuffer = rResCommands.CreateTexture2D(m_environmentMapSize, m_environmentMapSize, RdrResourceFormat::D24_UNORM_S8_UINT, RdrResourceUsage::Default, nullptr);
		m_hEnvironmentMapDepthView = rResCommands.CreateDepthStencilView(m_hEnvironmentMapDepthBuffer);
	}

	// Camera
	m_cameraSpawnPosition = pSceneData->camPosition;
	m_cameraSpawnPitchYawRoll = pSceneData->camPitchYawRoll;

	// Objects/Entities
	for (const AssetLib::Object& rObjectData : pSceneData->objects)
	{
		Entity* pEntity = Entity::Create(rObjectData.name, rObjectData.position, rObjectData.orientation, rObjectData.scale);

		// Model
		if (rObjectData.modelName)
		{
			pEntity->AttachRenderable(ModelInstance::Create(rObjectData.modelName, rObjectData.materialSwaps, rObjectData.numMaterialSwaps));
		}

		// Physics
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
		pEntity->AttachRigidBody(pRigidBody);

		// Lighting
		switch (rObjectData.light.type)
		{
		case LightType::Directional:
			pEntity->AttachLight(Light::CreateDirectional(rObjectData.light.color, rObjectData.light.intensity, rObjectData.light.pssmLambda));
			break;
		case LightType::Point:
			pEntity->AttachLight(Light::CreatePoint(rObjectData.light.color, rObjectData.light.intensity, rObjectData.light.radius));
			break;
		case LightType::Spot:
			pEntity->AttachLight(Light::CreateSpot(rObjectData.light.color, rObjectData.light.intensity, rObjectData.light.radius, rObjectData.light.innerConeAngle, rObjectData.light.outerConeAngle));
			break;
		case LightType::Environment:
			{
				Light* pLight = Light::CreateEnvironment(rObjectData.light.bIsGlobalEnvironmentLight);
				pEntity->AttachLight(pLight);
				CaptureEnvironmentLight(pLight);
			}
			break;
		}

		// Volume component
		switch (rObjectData.volume.volumeType)
		{
		case AssetLib::VolumeType::kPostProcess:
			pEntity->AttachVolume(PostProcessVolume::Create(rObjectData.volume));
			break;
		case AssetLib::VolumeType::kSky:
			pEntity->AttachVolume(SkyVolume::Create(rObjectData.volume));
			break;
		}

		m_entities.push_back(pEntity);
	}

	if (pSceneData->terrain.enabled)
	{
		m_terrain.Init(pSceneData->terrain);
	}

	// TODO: quad/oct tree for scene
}

void Scene::Update()
{
	if (m_reloadPending)
	{
		CachedString sceneName = m_sceneName;
		Cleanup();
		Load(sceneName.getString());
		m_reloadPending = false;
	}

	for (Entity* pEntity : m_entities)
	{
		pEntity->Update();
	}
}

void Scene::AddEntity(Entity* pEntity)
{
	m_entities.push_back(pEntity);
}

void Scene::RemoveEntity(Entity* pEntity)
{
	auto iter = std::find(m_entities.begin(), m_entities.end(), pEntity);
	if (iter != m_entities.end())
	{
		m_entities.erase(iter);
	}
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
