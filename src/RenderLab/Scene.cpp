#include "Precompiled.h"
#include "Scene.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "WorldObject.h"
#include "render/ModelInstance.h"
#include "render/Font.h"
#include "AssetLib/SceneAsset.h"

namespace
{
	void handleSceneFileChanged(const char* filename, void* pUserData)
	{
		char sceneName[AssetLib::AssetDef::kMaxNameLen];
		AssetLib::g_sceneDef.ExtractAssetName(filename, sceneName, ARRAY_SIZE(sceneName));

		Scene* pScene = (Scene*)pUserData;
		if (_stricmp(pScene->GetName(), sceneName) == 0)
		{
			pScene->Reload();
		}
	}

	uint s_stressTestLights = 0;
	void spawnLightStressTest(LightList& rLightList)
	{
		for (float x = -150.f; x < 150.f; x += 8.f)
		{
			for (float y = -150.f; y < 150.f; y += 8.f)
			{
				Light light;
				light.type = LightType::Point;
				light.color = Vec3(800.f, 0.f, 0.f);
				light.position.x = x;
				light.position.y = 5.f;
				light.position.z = y;
				light.radius = 16.f;
				light.castsShadows = false;
				rLightList.AddLight(light);
			}
		}
	}
}

Scene::Scene()
	: m_reloadListenerId(0)
	, m_reloadPending(false)
{
	m_sceneName[0] = 0;
}

void Scene::Cleanup()
{
	uint numObjects = (uint)m_objects.size();
	for (uint i = 0; i < numObjects; ++i)
	{
		m_objects[i]->Release();
	}
	m_objects.clear();

	m_lights.Cleanup();

	m_sky.Cleanup();

	FileWatcher::RemoveListener(m_reloadListenerId);
	m_sceneName[0] = 0;
}

void Scene::Reload()
{
	m_reloadPending = true;
}

void Scene::Load(const char* sceneName)
{
	assert(m_sceneName[0] == 0);
	strcpy_s(m_sceneName, sceneName);

	char* pFileData;
	uint fileSize;
	if (!AssetLib::g_sceneDef.LoadAsset(sceneName, &pFileData, &fileSize))
	{
		assert(false);
		return;
	}

	AssetLib::Scene* pSceneData = AssetLib::Scene::FromMem(pFileData);

	// Listen for changes of the scene file.
	char filePattern[AssetLib::AssetDef::kMaxNameLen];
	AssetLib::g_sceneDef.GetFilePattern(filePattern, ARRAY_SIZE(filePattern));
	m_reloadListenerId = FileWatcher::AddListener(filePattern, handleSceneFileChanged, this);

	// Camera - Skip moving the camera if this is a reload
	if (!m_reloadPending)
	{
		m_mainCamera.SetPosition(pSceneData->camPosition);
		m_mainCamera.SetPitchYawRoll(pSceneData->camPitchYawRoll);
	}

	// Sky
	m_sky.Load(pSceneData->sky);

	// Post-processing effects
	{
		char* pPostProcFileData;
		uint postProcFileSize;
		if (!AssetLib::g_postProcessEffectsDef.LoadAsset(pSceneData->postProcessingEffects, &pPostProcFileData, &postProcFileSize))
		{
			assert(false);
			return;
		}

		m_postProcEffects.Init(AssetLib::PostProcessEffects::FromMem(pPostProcFileData));

		delete pPostProcFileData;
	}

	// Lights
	{
		uint numLights = pSceneData->lightCount;
		for (uint i = 0; i < numLights; ++i)
		{
			const AssetLib::Light& rLightData = pSceneData->lights.ptr[i];

			Light light;
			light.type = rLightData.type;
			light.color = rLightData.color;
			light.position = rLightData.position;
			light.direction = rLightData.direction;
			light.radius = rLightData.radius;
			light.innerConeAngleCos = rLightData.innerConeAngleCos;
			light.outerConeAngleCos = rLightData.outerConeAngleCos;
			light.castsShadows = rLightData.bCastsShadows;

			m_lights.AddLight(light);
		}

		if (s_stressTestLights)
		{
			spawnLightStressTest(m_lights);
		}
	}

	// Objects
	{
		uint numObjects = pSceneData->objectCount;

		for (uint i = 0; i < numObjects; ++i)
		{
			const AssetLib::Object& rObjectData = pSceneData->objects.ptr[i];
			ModelInstance* pModel = ModelInstance::Create(rObjectData.model);
			m_objects.push_back(WorldObject::Create(pModel, rObjectData.position, rObjectData.orientation, rObjectData.scale));
		}
	}

	// TODO: quad/oct tree for scene

	delete pFileData;
}

void Scene::Update(float dt)
{
	if (m_reloadPending)
	{
		char sceneName[AssetLib::AssetDef::kMaxNameLen];
		strcpy_s(sceneName, m_sceneName);

		Cleanup();
		Load(sceneName);
		m_reloadPending = false;
	}

	m_sky.Update(dt);
}

void Scene::PrepareDraw()
{
	for (uint i = 0; i < m_objects.size(); ++i)
	{
		m_objects[i]->PrepareDraw();
	}
	m_sky.PrepareDraw();
	m_postProcEffects.PrepareDraw();
}
