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
	pScene->environmentMapTexSize = jRoot.get("environmentMapTexSize", 128).asUInt();

	Json::Value jCamera = jRoot.get("camera", Json::Value::null);

	Json::Value jPos = jCamera.get("position", Json::Value::null);
	pScene->camPosition = jsonReadVec3(jPos);

	Json::Value jRot = jCamera.get("rotation", Json::Value::null);
	pScene->camRotation = jsonReadRotation(jRot);

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

	// Ocean
	{
		Json::Value jOcean = jRoot.get("ocean", Json::Value::null);
		AssetLib::Ocean& rOcean = pScene->ocean;

		if (jOcean.isNull())
		{
			rOcean.enabled = false;
		}
		else
		{
			rOcean.enabled = true;

			rOcean.fourierGridSize = jOcean.get("fourierGridSize", 64).asInt();
			rOcean.tileWorldSize = jOcean.get("tileWorldSize", 64.f).asFloat();
			rOcean.tileCounts = jsonReadUVec2(jOcean.get("tileCounts", Json::Value::null));
			rOcean.waveHeightScalar = jOcean.get("waveHeightScalar", 0.000015f).asFloat();
			rOcean.wind = jsonReadVec2(jOcean.get("wind", Json::Value::null));
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
			rObj.rotation = jsonReadRotation(jObj.get("rotation", Json::Value::null));
			rObj.scale = jsonReadScale(jObj.get("scale", Json::Value::null));
			jsonReadString(jObj.get("name", Json::Value::null), rObj.name, ARRAY_SIZE(rObj.name));

			//////////////////////////////////////////////////////////////////////////
			// Model component
			Json::Value jModel = jObj.get("model", Json::Value::null);
			if (jModel.isString())
			{
				jsonReadCachedString(jModel, &rObj.model.name);
			}
			else if (jModel.isObject())
			{
				jsonReadCachedString(jModel.get("name", Json::Value::null), &rObj.model.name);

				// Material swaps
				Json::Value jMaterialSwaps = jModel.get("materialSwaps", Json::Value::null);
				if (jMaterialSwaps.isObject())
				{
					int numSwaps = jMaterialSwaps.size();
					Assert(numSwaps < ARRAY_SIZE(rObj.model.materialSwaps));
					for (auto iter = jMaterialSwaps.begin(); iter != jMaterialSwaps.end(); ++iter)
					{
						rObj.model.materialSwaps[rObj.model.numMaterialSwaps].from = iter.name().c_str();
						jsonReadCachedString(*iter, &rObj.model.materialSwaps[rObj.model.numMaterialSwaps].to);
						++rObj.model.numMaterialSwaps;
					}
				}
			}

			//////////////////////////////////////////////////////////////////////////
			// Decal
			Json::Value jDecal = jObj.get("decal", Json::Value::null);
			if (jDecal.isObject())
			{
				jsonReadCachedString(jDecal.get("texture", Json::Value::null), &rObj.decal.textureName);
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
					rLight.radius = FLT_MAX;

					rLight.direction = jsonReadVec3(jLight.get("direction", Json::Value::null));
					rLight.direction = Vec3Normalize(rLight.direction);

					rLight.pssmLambda = jLight.get("pssmLambda", 0.6f).asFloat();
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
					Assert(false);
				}

				rLight.color = jsonReadVec3(jLight.get("color", Json::Value::null));
				rLight.intensity = jLight.get("intensity", 1.f).asFloat();
				rLight.bCastsShadows = jLight.get("castsShadows", false).asBool();
			}

			//////////////////////////////////////////////////////////////////////////
			// Volume component
			Json::Value jVolume = jObj.get("volume", Json::Value::null);
			if (jVolume.isObject())
			{
				Json::Value jVolumeType = jVolume.get("type", Json::Value::null);
				const char* strVolumeType = jVolumeType.asCString();

				if (_stricmp(strVolumeType, "sky") == 0)
				{
					const SkySettings defaultSky = Volume::MakeDefaultSky().skySettings;

					rObj.volume.volumeType = VolumeType::kSky;
					SkySettings& rSky = rObj.volume.skySettings;

					// Volumetric fog
					Json::Value jFog = jVolume.get("volumetricFog", Json::Value::null);
					rSky.volumetricFog.enabled = jFog.get("enabled", defaultSky.volumetricFog.enabled).asBool();
					rSky.volumetricFog.scatteringCoeff = jsonReadVec3(jFog.get("scatteringCoeff", Json::Value::null));
					rSky.volumetricFog.absorptionCoeff = jsonReadVec3(jFog.get("absorptionCoeff", Json::Value::null));
					rSky.volumetricFog.phaseG = jFog.get("phaseG", defaultSky.volumetricFog.phaseG).asFloat();
					rSky.volumetricFog.farDepth = jFog.get("farDepth", defaultSky.volumetricFog.farDepth).asFloat();
				}
				else if (_stricmp(strVolumeType, "postProcess") == 0)
				{
					const PostProcessEffects defaultPostProc = Volume::MakeDefaultPostProcess().postProcessEffects;

					rObj.volume.volumeType = VolumeType::kPostProcess;
					PostProcessEffects& rEffects = rObj.volume.postProcessEffects;

					// Bloom
					Json::Value jBloom = jVolume.get("bloom", Json::Value::null);
					rEffects.bloom.enabled = jBloom.get("enabled", defaultPostProc.bloom.enabled).asBool();
					rEffects.bloom.threshold = jBloom.get("threshold", defaultPostProc.bloom.threshold).asFloat();

					// SSAO
					Json::Value jSsao = jVolume.get("ssao", Json::Value::null);
					rEffects.ssao.enabled = jSsao.get("enabled", defaultPostProc.ssao.enabled).asBool();
					rEffects.ssao.sampleRadius = jSsao.get("sampleRadius", defaultPostProc.ssao.sampleRadius).asFloat();

					// Eye adaptation
					Json::Value jEyeAdaptation = jVolume.get("eyeAdaptation", Json::Value::null);
					rEffects.eyeAdaptation.white = jEyeAdaptation.get("white", defaultPostProc.eyeAdaptation.white).asFloat();
					rEffects.eyeAdaptation.middleGrey = jEyeAdaptation.get("middleGrey", defaultPostProc.eyeAdaptation.middleGrey).asFloat();
					rEffects.eyeAdaptation.minExposure = jEyeAdaptation.get("minExposure", defaultPostProc.eyeAdaptation.minExposure).asFloat();
					rEffects.eyeAdaptation.maxExposure = jEyeAdaptation.get("maxExposure", defaultPostProc.eyeAdaptation.maxExposure).asFloat();
					rEffects.eyeAdaptation.adaptationSpeed = jEyeAdaptation.get("adaptationSpeed", defaultPostProc.eyeAdaptation.adaptationSpeed).asFloat();
				}
			}
		}
	}

	return pScene;
}

Volume Volume::MakeDefaultSky()
{
	AssetLib::Volume volume;
	volume.volumeType = AssetLib::VolumeType::kSky;
	volume.skySettings.volumetricFog.enabled = false;

	volume.skySettings.volumetricFog.phaseG = 0.f;
	volume.skySettings.volumetricFog.farDepth = 0.f;
	volume.skySettings.volumetricFog.absorptionCoeff = Vec3::kZero;
	volume.skySettings.volumetricFog.scatteringCoeff = Vec3::kZero;

	return volume;
}

Volume Volume::MakeDefaultPostProcess()
{
	Volume volume;

	volume.volumeType = AssetLib::VolumeType::kPostProcess;
	volume.postProcessEffects.bloom.enabled = true;
	volume.postProcessEffects.bloom.threshold = 5.5f;

	volume.postProcessEffects.eyeAdaptation.white = 12.5f;
	volume.postProcessEffects.eyeAdaptation.middleGrey = 0.6f;
	volume.postProcessEffects.eyeAdaptation.minExposure = -16.f;
	volume.postProcessEffects.eyeAdaptation.maxExposure = 16.f;
	volume.postProcessEffects.eyeAdaptation.adaptationSpeed = 1.f;

	volume.postProcessEffects.ssao.enabled = true;
	volume.postProcessEffects.ssao.sampleRadius = 1.f;

	return volume;
}