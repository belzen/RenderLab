#include "SceneAsset.h"
#include "UtilsLib/Util.h"
#include "UtilsLib/JsonUtils.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Error.h"

using namespace AssetLib;

AssetDef& Scene::GetAssetDef()
{
	static AssetLib::AssetDef s_assetDef("scenes", "scene", 1);
	return s_assetDef;
}

Scene* Scene::Load(const CachedString& assetName, Scene* pScene)
{
	Json::Value jRoot;
	if (!GetAssetDef().LoadAssetJson(assetName.getString(), &jRoot))
	{
		Error("Failed to load scene asset: %s", assetName.getString());
		return pScene;
	}

	if (!pScene)
	{
		pScene = new Scene();
	}

	pScene->assetName = assetName;

	Json::Value jCamera = jRoot.get("camera", Json::Value::null);

	Json::Value jPos = jCamera.get("position", Json::Value::null);
	pScene->camPosition = jsonReadVec3(jPos);

	Json::Value jRot = jCamera.get("rotation", Json::Value::null);
	pScene->camPitchYawRoll = jsonReadPitchYawRoll(jRot);

	// Sky & Post-processing effects
	jsonReadCachedString(jRoot.get("sky", Json::Value::null), &pScene->skyName);
	jsonReadCachedString(jRoot.get("postProcessingEffects", Json::Value::null), &pScene->postProcessingEffectsName);

	// Terrain
	{
		Json::Value jTerrain = jRoot.get("terrain", Json::Value::null);
		AssetLib::Terrain& rTerrain = pScene->terrain;

		if (jTerrain.isNull())
		{
			rTerrain.enabled = false;
		}
		else
		{
			rTerrain.enabled = true;
		
			rTerrain.cornerMin = jsonReadVec2(jTerrain.get("cornerMin", Json::Value::null));
			rTerrain.cornerMax = jsonReadVec2(jTerrain.get("cornerMax", Json::Value::null));
			rTerrain.heightScale = jTerrain.get("heightScale", 1.f).asFloat();
			jsonReadCachedString(jTerrain.get("heightmap", Json::Value::null), &rTerrain.heightmapName);
		}
	}

	// Lights
	{
		Json::Value jLights = jRoot.get("lights", Json::Value::null);

		uint numLights = jLights.size();
		pScene->lights.resize(jLights.size());

		for (uint i = 0; i < numLights; ++i)
		{
			Json::Value jLight = jLights.get(i, Json::Value::null);
			AssetLib::Light& rLight = pScene->lights[i];

			Json::Value jType = jLight.get("type", Json::Value::null);
			const char* typeStr = jType.asCString();
			if (_stricmp(typeStr, "directional") == 0)
			{
				rLight.type = LightType::Directional;

				rLight.direction = jsonReadVec3(jLight.get("direction", Json::Value::null));
				rLight.direction = Vec3Normalize(rLight.direction);
			}
			else if (_stricmp(typeStr, "spot") == 0)
			{
				rLight.type = LightType::Spot;

				rLight.direction = jsonReadVec3(jLight.get("direction", Json::Value::null));
				rLight.direction = Vec3Normalize(rLight.direction);

				float innerConeAngle = jLight.get("innerConeAngle", 0.f).asFloat();
				rLight.innerConeAngle = Maths::DegToRad(innerConeAngle);

				float outerConeAngle = jLight.get("outerConeAngle", 0.f).asFloat();
				rLight.outerConeAngle = Maths::DegToRad(outerConeAngle);
			}
			else if (_stricmp(typeStr, "point") == 0)
			{
				rLight.type = LightType::Point;
			}
			else if (_stricmp(typeStr, "environment") == 0)
			{
				rLight.type = LightType::Environment;
			}
			else
			{
				assert(false);
				delete pScene;
				return nullptr;
			}

			rLight.position = jsonReadVec3(jLight.get("position", Json::Value::null));

			rLight.color = jsonReadVec3(jLight.get("color", Json::Value::null));
			float intensity = jLight.get("intensity", 1.f).asFloat();
			rLight.color.x *= intensity;
			rLight.color.y *= intensity;
			rLight.color.z *= intensity;

			rLight.radius = jLight.get("radius", 0.f).asFloat();
			if (rLight.type == LightType::Directional)
			{
				rLight.radius = FLT_MAX;
			}

			rLight.bCastsShadows = jLight.get("castsShadows", false).asBool();
		}
	}

	// Global environment light
	{
		Json::Value jGlobalEnvLight = jRoot.get("globalEnvironmentLight", Json::Value::null);
		pScene->globalEnvironmentLightPosition = jsonReadVec3(jGlobalEnvLight.get("position", Json::Value::null));
	}

	// Objects
	{
		Json::Value jObjects = jRoot.get("objects", Json::Value::null);

		uint numObjects = jObjects.size();
		pScene->objects.resize(jObjects.size());

		for (uint i = 0; i < numObjects; ++i)
		{
			Json::Value jObj = jObjects.get(i, Json::Value::null);
			AssetLib::Object& rObj = pScene->objects[i];

			rObj.position = jsonReadVec3(jObj.get("position", Json::Value::null));
			rObj.orientation = jsonReadRotation(jObj.get("rotation", Json::Value::null));
			rObj.scale = jsonReadScale(jObj.get("scale", Json::Value::null));
			jsonReadCachedString(jObj.get("model", Json::Value::null), &rObj.modelName);
			jsonReadString(jObj.get("name", Json::Value::null), rObj.name, ARRAY_SIZE(rObj.name));

			Json::Value jMaterialSwaps = jObj.get("materialSwaps", Json::Value::null);
			if (jMaterialSwaps.isObject())
			{
				int numSwaps = jMaterialSwaps.size();
				assert(numSwaps < ARRAY_SIZE(rObj.materialSwaps));
				for (auto iter = jMaterialSwaps.begin(); iter != jMaterialSwaps.end(); ++iter)
				{
					rObj.materialSwaps[rObj.numMaterialSwaps].from = iter.name().c_str();
					jsonReadCachedString(*iter, &rObj.materialSwaps[rObj.numMaterialSwaps].to);
					++rObj.numMaterialSwaps;
				}
			}
		}
	}

	return pScene;
}
