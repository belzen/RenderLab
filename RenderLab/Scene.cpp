#include "Precompiled.h"
#include "Scene.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "WorldObject.h"
#include "render/Model.h"
#include "render/Font.h"
#include "json/json.h"
#include "FileWatcher.h"
#include "FileLoader.h"

namespace
{
	void handleSceneFileChanged(const char* filename, void* pUserData)
	{
		/*
		Scene* pScene = (Scene*)pUserData;
		if (stricmp(filename, pScene->GetFilename()) == 0)
		{
			pScene->Load()
		}*/
	}
}

Scene::Scene()
{
	FileWatcher::AddListener("scenes/*.scene", handleSceneFileChanged, this);
}

static inline Vec3 readVec3(Json::Value& val)
{
	Vec3 vec;
	vec.x = val.get((uint)0, Json::Value(0.f)).asFloat();
	vec.y = val.get((uint)1, Json::Value(0.f)).asFloat();
	vec.z = val.get((uint)2, Json::Value(0.f)).asFloat();
	return vec;
}

static inline Vec3 readScale(Json::Value& val)
{
	Vec3 vec;
	vec.x = val.get((uint)0, Json::Value(1.f)).asFloat();
	vec.y = val.get((uint)1, Json::Value(1.f)).asFloat();
	vec.z = val.get((uint)2, Json::Value(1.f)).asFloat();
	return vec;
}

static inline Quaternion readRotation(Json::Value& val)
{
	Vec3 pitchYawRoll;
	pitchYawRoll.x = val.get((uint)0, Json::Value(0.f)).asFloat();
	pitchYawRoll.y = val.get((uint)1, Json::Value(0.f)).asFloat();
	pitchYawRoll.z = val.get((uint)2, Json::Value(0.f)).asFloat();

	return QuaternionPitchYawRoll(
		Maths::DegToRad(pitchYawRoll.x), 
		Maths::DegToRad(pitchYawRoll.y), 
		Maths::DegToRad(pitchYawRoll.z));
}

static inline Vec3 readPitchYawRoll(Json::Value& val)
{
	Vec3 pitchYawRoll;
	pitchYawRoll.x = Maths::DegToRad(val.get((uint)0, Json::Value(0.f)).asFloat());
	pitchYawRoll.y = Maths::DegToRad(val.get((uint)1, Json::Value(0.f)).asFloat());
	pitchYawRoll.z = Maths::DegToRad(val.get((uint)2, Json::Value(0.f)).asFloat());
	return pitchYawRoll;
}

void Scene::Load(Renderer& rRenderer, const char* filename)
{
	char fullFilename[MAX_PATH];
	sprintf_s(fullFilename, "data/scenes/%s", filename);


	Json::Value root;
	FileLoader::LoadJson(fullFilename, root);

	// Camera
	{
		Json::Value jCamera = root.get("camera", Json::Value::null);

		Json::Value jPos = jCamera.get("position", Json::Value::null);
		m_mainCamera.SetPosition(readVec3(jPos));

		Json::Value jRot = jCamera.get("rotation", Json::Value::null);
		m_mainCamera.SetPitchYawRoll(readPitchYawRoll(jRot));
	}

	// Sky
	{
		Json::Value jSky = root.get("sky", Json::Value::null);
		m_sky.LoadFromFile(jSky.asCString());
	}

	// Lights
	{
		Json::Value jLights = root.get("lights", Json::Value::null);
		int numLights = jLights.size();

		for (int i = 0; i < numLights; ++i)
		{
			Json::Value jLight = jLights.get(i, Json::Value::null);

			Light light;

			Json::Value jType = jLight.get("type", Json::Value::null);
			const char* typeStr = jType.asCString();
			if (_stricmp(typeStr, "directional") == 0)
			{
				light.type = LightType::Directional;

				light.direction = readVec3(jLight.get("direction", Json::Value::null));
				light.direction = Vec3Normalize(light.direction);
			}
			else if (_stricmp(typeStr, "spot") == 0)
			{
				light.type = LightType::Spot;

				light.direction = readVec3(jLight.get("direction", Json::Value::null));
				light.direction = Vec3Normalize(light.direction);

				float innerConeAngle = jLight.get("innerConeAngle", 0.f).asFloat();
				light.innerConeAngleCos = cosf((innerConeAngle / 180.f) * Maths::kPi);

				float outerConeAngle = jLight.get("outerConeAngle", 0.f).asFloat();
				light.outerConeAngleCos = cosf((outerConeAngle / 180.f) * Maths::kPi);
			}
			else if (_stricmp(typeStr, "point") == 0)
			{
				light.type = LightType::Point;
			}
			else
			{
				assert(false);
			}

			light.position = readVec3(jLight.get("position", Json::Value::null));

			light.color = readVec3(jLight.get("color", Json::Value::null));
			float intensity = jLight.get("intensity", 1.f).asFloat();
			light.color.x *= intensity;
			light.color.y *= intensity;
			light.color.z *= intensity;

			light.radius = jLight.get("radius", 0.f).asFloat();
			if (light.type == LightType::Directional)
			{
				light.radius = FLT_MAX;
			}

			light.castsShadows = jLight.get("castsShadows", false).asBool();

			m_lights.AddLight(light);
		}
	}

	// Objects
	{
		Json::Value jObjects = root.get("objects", Json::Value::null);
		int numObjects = jObjects.size();

		for (int i = 0; i < numObjects; ++i)
		{
			Json::Value jObj = jObjects.get(i, Json::Value::null);

			Vec3 pos = readVec3(jObj.get("position", Json::Value::null));
			Quaternion orientation = readRotation(jObj.get("rotation", Json::Value::null));
			Vec3 scale = readScale(jObj.get("scale", Json::Value::null));
			
			Json::Value jModel = jObj.get("geo", Json::Value::null);
			RdrGeoHandle hGeo = RdrGeoSystem::CreateGeoFromFile(jModel.asCString(), nullptr);

			Json::Value jMaterialName = jObj.get("material", Json::Value::null);
			const RdrMaterial* pMaterial = RdrMaterial::LoadFromFile(jMaterialName.asCString());

			Model* pModel = Model::Create(hGeo, pMaterial);
			m_objects.push_back(WorldObject::Create(pModel, pos, orientation, scale));
		}
	}

	// TODO: quad/oct tree for scene
}

void Scene::Update(float dt)
{
}

void Scene::QueueShadowMaps(Renderer& rRenderer, const Camera& rCamera)
{
	m_lights.PrepareDrawForScene(rRenderer, rCamera, *this);
}

void Scene::QueueDraw(Renderer& rRenderer) const
{
	for (uint i = 0; i < m_objects.size(); ++i)
	{
		m_objects[i]->QueueDraw(rRenderer);
	}
	m_sky.QueueDraw(rRenderer);
}
