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

			//////////////////////////////////////////////////////////////////////////
			// Physics component
			Json::Value jPhysics = jObj.get("physics", Json::Value::null);
			if (jPhysics.isObject())
			{
				Json::Value jShape = jPhysics.get("shape", Json::Value::null);
				const char* shapeStr = jShape.asCString();
				if (_stricmp(shapeStr, "box") == 0)
				{
					rObj.physics.shape = ShapeType::Box;
					rObj.physics.halfSize = jsonReadVec3(jPhysics.get("halfSize", Json::Value::null));
				}
				else if (_stricmp(shapeStr, "sphere") == 0)
				{
					rObj.physics.shape = ShapeType::Sphere;
					rObj.physics.halfSize.x = jPhysics.get("radius", 1.f).asFloat();
				}
				else
				{
					rObj.physics.shape = ShapeType::None;
				}

				rObj.physics.density = jPhysics.get("density", 0.f).asFloat();
				rObj.physics.offset = jsonReadVec3(jPhysics.get("offset", Json::Value::null));
			}

			//////////////////////////////////////////////////////////////////////////
			// Light component
			Json::Value jLight = jObj.get("light", Json::Value::null);
			if (jLight.isObject())
			{
				AssetLib::Light& rLight = rObj.light;

				Json::Value jType = jLight.get("type", Json::Value::null);
				const char* typeStr = jType.asCString();
				if (_stricmp(typeStr, "directional") == 0)
				{
					rLight.type = LightType::Directional;

					rLight.direction = jsonReadVec3(jLight.get("direction", Json::Value::null));
					rLight.direction = Vec3Normalize(rLight.direction);
					rLight.radius = FLT_MAX;
				}
				else if (_stricmp(typeStr, "spot") == 0)
				{
					rLight.type = LightType::Spot;
					rLight.radius = jLight.get("radius", 0.f).asFloat();

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
					rLight.radius = jLight.get("radius", 0.f).asFloat();
				}
				else if (_stricmp(typeStr, "environment") == 0)
				{
					rLight.type = LightType::Environment;
					rLight.bIsGlobalEnvironmentLight = jLight.get("isGlobalEnvironmentLight", false).asBool();
				}
				else
				{
					assert(false);
				}

				float intensity = jLight.get("intensity", 1.f).asFloat();
				rLight.color = intensity * jsonReadVec3(jLight.get("color", Json::Value::null));
				rLight.bCastsShadows = jLight.get("castsShadows", false).asBool();
			}
		}
	}

	return pScene;
}
