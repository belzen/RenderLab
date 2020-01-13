#include "Precompiled.h"
#include "Scene.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "render/Font.h"
#include "render/RdrOffscreenTasks.h"
#include "Entity.h"
#include "AssetLib/SceneAsset.h"
#include "AssetLib/AssetLibrary.h"
#include "render/Terrain.h"
#include "render/Ocean.h"
#include "Game.h"

namespace
{
	struct
	{
		DefaultComponentAllocator m_componentAllocator;
		EntityList m_entities;
		Terrain m_terrain;
		Ocean m_ocean;

		Vec3 m_cameraSpawnPosition;
		Rotation m_cameraSpawnRotation;

		Light* m_apActiveEnvironmentLights[MAX_ENVIRONMENT_MAPS];
		RdrResourceHandle m_hEnvironmentMapTexArray;
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

		Assert(s_scene.m_apActiveEnvironmentLights[index] == pLight);
		Rect viewport(0.f, 0.f, (float)s_scene.m_environmentMapSize, (float)s_scene.m_environmentMapSize);
		RdrOffscreenTasks::QueueSpecularProbeCapture(pLight->GetEntity()->GetPosition(), viewport,
			s_scene.m_hEnvironmentMapTexArray, index);
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
		Assert(false);
		return;
	}

	memset(s_scene.m_apActiveEnvironmentLights, 0, sizeof(s_scene.m_apActiveEnvironmentLights));
	if (!s_scene.m_hEnvironmentMapTexArray || s_scene.m_environmentMapSize != pSceneData->environmentMapTexSize)
	{
		RdrResourceCommandList& rResCommands = g_pRenderer->GetResourceCommandList();
		if (s_scene.m_hEnvironmentMapTexArray)
		{
			rResCommands.ReleaseResource(s_scene.m_hEnvironmentMapTexArray, CREATE_NULL_BACKPOINTER);
			s_scene.m_hEnvironmentMapTexArray = 0;
		}

		s_scene.m_environmentMapSize = pSceneData->environmentMapTexSize;
		s_scene.m_hEnvironmentMapTexArray = RdrResourceSystem::CreateTextureCubeArray(s_scene.m_environmentMapSize, s_scene.m_environmentMapSize, MAX_ENVIRONMENT_MAPS, 
			RdrResourceFormat::R16G16B16A16_FLOAT, RdrResourceAccessFlags::CpuRO_GpuRO_RenderTarget, CREATE_NULL_BACKPOINTER);
	}

	// Camera
	s_scene.m_cameraSpawnPosition = pSceneData->camPosition;
	s_scene.m_cameraSpawnRotation = pSceneData->camRotation;

	// Objects/Entities
	bool bHasPostProcessVolume = false;
	bool bHasSkyVolume = false;
	for (const AssetLib::Object& rObjectData : pSceneData->objects)
	{
		Entity* pEntity = Entity::Create(rObjectData.name, rObjectData.position, rObjectData.rotation, rObjectData.scale);

		// Model
		if (rObjectData.model.name)
		{
			pEntity->AttachRenderable(ModelComponent::Create(&s_scene.m_componentAllocator, rObjectData.model.name, rObjectData.model.materialSwaps, rObjectData.model.numMaterialSwaps));
		}
		else if (rObjectData.decal.textureName)
		{
			pEntity->AttachRenderable(Decal::Create(&s_scene.m_componentAllocator, rObjectData.decal.textureName));
		}

		// Physics
		RigidBody* pRigidBody = nullptr;
		switch (rObjectData.physics.shape)
		{
		case AssetLib::ShapeType::Box:
			pRigidBody = RigidBody::CreateBox(&s_scene.m_componentAllocator, PhysicsGroup::kDefault, rObjectData.physics.halfSize, rObjectData.physics.density, rObjectData.physics.offset);
			break;
		case AssetLib::ShapeType::Sphere:
			pRigidBody = RigidBody::CreateSphere(&s_scene.m_componentAllocator, PhysicsGroup::kDefault, rObjectData.physics.halfSize.x, rObjectData.physics.density, rObjectData.physics.offset);
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
			bHasPostProcessVolume = true;
			break;
		case AssetLib::VolumeType::kSky:
			pEntity->AttachVolume(SkyVolume::Create(&s_scene.m_componentAllocator, rObjectData.volume));
			bHasSkyVolume = true;
			break;
		}

		s_scene.m_entities.push_back(pEntity);
	}

	if (!bHasSkyVolume)
	{
		Entity* pEntity = Entity::Create("DefaultSky", Vec3::kZero, Rotation::kIdentity, Vec3::kOne);
		pEntity->AttachVolume(SkyVolume::Create(&s_scene.m_componentAllocator, AssetLib::Volume::MakeDefaultSky()));
		s_scene.m_entities.push_back(pEntity);
	}
	if (!bHasPostProcessVolume)
	{
		Entity* pEntity = Entity::Create("DefaultPostProcess", Vec3::kZero, Rotation::kIdentity, Vec3::kOne);
		pEntity->AttachVolume(PostProcessVolume::Create(&s_scene.m_componentAllocator, AssetLib::Volume::MakeDefaultPostProcess()));
		s_scene.m_entities.push_back(pEntity);
	}

	if (pSceneData->terrain.enabled)
	{
		s_scene.m_terrain.Init(pSceneData->terrain);
	}

	if (pSceneData->ocean.enabled)
	{
		const AssetLib::Ocean& rOcean = pSceneData->ocean;
		s_scene.m_ocean.Init(rOcean.tileWorldSize, rOcean.tileCounts, rOcean.fourierGridSize, rOcean.waveHeightScalar, rOcean.wind);
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

	s_scene.m_ocean.Update();
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

void Scene::QueueDraw(RdrAction* pAction)
{
	const Camera& rCamera = pAction->GetCamera();
	Vec3 camDir = rCamera.GetDirection();
	Vec3 camPos = rCamera.GetPosition();
	float depthMin = FLT_MAX;
	float depthMax = 0.f;
	RdrDrawOpSet opSet;

	//////////////////////////////////////////////////////////////////////////
	// Models
	for (ModelComponent& rModel : s_scene.m_componentAllocator.GetModelComponentFreeList())
	{
		Entity* pEntity = rModel.GetEntity();
		float radius = rModel.GetRadius();
		if (!rCamera.CanSee(pEntity->GetPosition(), radius))
		{
			// Can't see the model.
			continue;
		}

		opSet = rModel.BuildDrawOps(pAction);
		for (uint16 i = 0; i < opSet.numDrawOps; ++i)
		{
			RdrDrawOp* pDrawOp = &opSet.aDrawOps[i];
			if (pDrawOp->bHasAlpha)
			{
				pAction->AddDrawOp(pDrawOp, RdrBucketType::Alpha);
			}
			else
			{
				pAction->AddDrawOp(pDrawOp, RdrBucketType::ZPrepass);
				pAction->AddDrawOp(pDrawOp, RdrBucketType::Opaque);
			}
		}

		Vec3 diff = pEntity->GetPosition() - camPos;
		float distSqr = Vec3Dot(camDir, diff);
		float dist = sqrtf(max(0.f, distSqr));

		if (dist - radius < depthMin)
			depthMin = dist - radius;

		if (dist + radius > depthMax)
			depthMax = dist + radius;
	}

	//////////////////////////////////////////////////////////////////////////
	// Decals
	for (Decal& rDecal : s_scene.m_componentAllocator.GetDecalFreeList())
	{
		Entity* pEntity = rDecal.GetEntity();
		float radius = rDecal.GetRadius();
		if (!rCamera.CanSee(pEntity->GetPosition(), radius))
		{
			// Can't see the decal.
			continue;
		}

		opSet = rDecal.BuildDrawOps(pAction);
		for (uint16 i = 0; i < opSet.numDrawOps; ++i)
		{
			RdrDrawOp* pDrawOp = &opSet.aDrawOps[i];
			pAction->AddDrawOp(pDrawOp, RdrBucketType::Decal);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Terrain
	opSet = s_scene.m_terrain.BuildDrawOps(pAction);
	for (uint16 i = 0; i < opSet.numDrawOps; ++i)
	{
		pAction->AddDrawOp(&opSet.aDrawOps[i], RdrBucketType::Opaque);
	}

	//////////////////////////////////////////////////////////////////////////
	// Ocean
	opSet = s_scene.m_ocean.BuildDrawOps(pAction);
	for (uint16 i = 0; i < opSet.numDrawOps; ++i)
	{
		pAction->AddDrawOp(&opSet.aDrawOps[i], RdrBucketType::Opaque);
	}

	//////////////////////////////////////////////////////////////////////////
	// Sky & Lighting
	for (Light& rLight : s_scene.m_componentAllocator.GetLightFreeList())
	{
		pAction->AddLight(&rLight);
	}

	AssetLib::SkySettings sky;
	for (SkyVolume& rSkyVolume : Scene::GetComponentAllocator()->GetSkyVolumeFreeList())
	{
		sky = rSkyVolume.GetSkySettings();
		// For now, only support one sky object.
		// TODO: Support blending of skies via volume changes and such.
		//		Will probably need to trim the SkyComponent down to mostly data and
		//		create a function/class that blends them together and maintains the render resources.	
		break;
	}

	depthMin = max(rCamera.GetNearDist(), depthMin);
	depthMax = min(rCamera.GetFarDist(), depthMax);
	pAction->QueueSkyAndLighting(sky, s_scene.m_hEnvironmentMapTexArray, depthMin, depthMax);

	//////////////////////////////////////////////////////////////////////////
	// Shadows
	// Can only be after lighting is queued else there are no shadow passes.
	// TODO: Better way to handle shadows.  Perhaps have the scene decide which lights will cast shadows this frame rather than RdrLighting.
	int numShadowPasses = pAction->GetShadowPassCount();
	for (ModelComponent& rModel : Scene::GetComponentAllocator()->GetModelComponentFreeList())
	{
		Entity* pEntity = rModel.GetEntity();
		RdrDrawOpSet ops;

		for (int iShadowPass = 0; iShadowPass < numShadowPasses; ++iShadowPass)
		{
			const Camera& rShadowCamera = pAction->GetShadowCamera(iShadowPass);
			if (rShadowCamera.CanSee(pEntity->GetPosition(), rModel.GetRadius()))
			{
				RdrBucketType shadowBucket = (RdrBucketType)((int)RdrBucketType::ShadowMap0 + iShadowPass);

				// Re-use draw ops if we've already built them for a previous shadow pass.
				if (ops.numDrawOps == 0)
				{
					ops = rModel.BuildDrawOps(pAction);
				}

				for (int i = 0; i < ops.numDrawOps; ++i)
				{
					if (!ops.aDrawOps[i].bHasAlpha)
					{
						pAction->AddDrawOp(&ops.aDrawOps[i], shadowBucket);
					}
				}
			}
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// Post-processing
	AssetLib::PostProcessEffects postProcFx;
	for (PostProcessVolume& rPostProcess : s_scene.m_componentAllocator.GetPostProcessVolumeFreeList())
	{
		postProcFx = rPostProcess.GetEffects();
		// TODO: Add post-process effects blending.
		break;
	}

	pAction->SetPostProcessingEffects(postProcFx);
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

EntityList& Scene::GetEntities()
{
	return s_scene.m_entities;
}

RdrResourceHandle Scene::GetEnvironmentMapTexArray()
{
	return s_scene.m_hEnvironmentMapTexArray;
}