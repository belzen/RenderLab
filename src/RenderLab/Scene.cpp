#include "Precompiled.h"
#include "Scene.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "render/Font.h"
#include "render/RdrOffscreenTasks.h"
#include "WorldObject.h"
#include "components/ModelInstance.h"
#include "components/Light.h"
#include "components/RigidBody.h"
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

	uint numObjects = (uint)m_objects.size();
	for (uint i = 0; i < numObjects; ++i)
	{
		m_objects[i]->Release();
	}
	m_objects.clear();

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
	RdrOffscreenTasks::QueueSpecularProbeCapture(pLight->GetParent()->GetPosition(), this, viewport,
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

		// Model
		if (rObjectData.modelName)
		{
			pObject->AttachModel(ModelInstance::Create(rObjectData.modelName, rObjectData.materialSwaps, rObjectData.numMaterialSwaps));
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
		pObject->AttachRigidBody(pRigidBody);

		// Lighting
		switch (rObjectData.light.type)
		{
		case LightType::Directional:
			pObject->AttachLight(Light::CreateDirectional(rObjectData.light.color, rObjectData.light.direction));
			break;
		case LightType::Point:
			pObject->AttachLight(Light::CreatePoint(rObjectData.light.color, rObjectData.light.radius));
			break;
		case LightType::Spot:
			pObject->AttachLight(Light::CreateSpot(rObjectData.light.color, rObjectData.light.direction, rObjectData.light.radius, rObjectData.light.innerConeAngle, rObjectData.light.outerConeAngle));
			break;
		case LightType::Environment:
			{
				Light* pLight = Light::CreateEnvironment(rObjectData.light.bIsGlobalEnvironmentLight);
				pObject->AttachLight(pLight);
				CaptureEnvironmentLight(pLight);
			}
			break;
		}

		m_objects.push_back(pObject);
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

void Scene::RemoveObject(WorldObject* pObject)
{
	auto iter = std::find(m_objects.begin(), m_objects.end(), pObject);
	if (iter != m_objects.end())
	{
		m_objects.erase(iter);
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
