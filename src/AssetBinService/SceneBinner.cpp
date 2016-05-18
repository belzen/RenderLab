#include "SceneBinner.h"
#include "AssetLib/SceneAsset.h"
#include "UtilsLib/FileLoader.h"
#include "UtilsLib/Error.h"
#include "UtilsLib/json/json.h"
#include "MathLib/Maths.h"

namespace
{
	inline Vec3 readVec3(Json::Value& val)
	{
		Vec3 vec;
		vec.x = val.get((uint)0, Json::Value(0.f)).asFloat();
		vec.y = val.get((uint)1, Json::Value(0.f)).asFloat();
		vec.z = val.get((uint)2, Json::Value(0.f)).asFloat();
		return vec;
	}

	inline Vec3 readScale(Json::Value& val)
	{
		Vec3 vec;
		vec.x = val.get((uint)0, Json::Value(1.f)).asFloat();
		vec.y = val.get((uint)1, Json::Value(1.f)).asFloat();
		vec.z = val.get((uint)2, Json::Value(1.f)).asFloat();
		return vec;
	}

	inline Quaternion readRotation(Json::Value& val)
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

	inline Vec3 readPitchYawRoll(Json::Value& val)
	{
		Vec3 pitchYawRoll;
		pitchYawRoll.x = Maths::DegToRad(val.get((uint)0, Json::Value(0.f)).asFloat());
		pitchYawRoll.y = Maths::DegToRad(val.get((uint)1, Json::Value(0.f)).asFloat());
		pitchYawRoll.z = Maths::DegToRad(val.get((uint)2, Json::Value(0.f)).asFloat());
		return pitchYawRoll;
	}
}

AssetLib::AssetDef& SceneBinner::GetAssetDef() const
{
	return AssetLib::g_sceneDef;
}

std::vector<std::string> SceneBinner::GetFileTypes() const
{
	std::vector<std::string> types;
	types.push_back("scene");
	return types;
}

void SceneBinner::CalcSourceHash(const std::string& srcFilename, Hashing::SHA1& rOutHash)
{
	char* pSrcFileData;
	uint srcFileSize;
	FileLoader::Load(srcFilename.c_str(), &pSrcFileData, &srcFileSize);

	Hashing::SHA1::Calculate(pSrcFileData, srcFileSize, rOutHash);

	delete pSrcFileData;
}

bool SceneBinner::BinAsset(const std::string& srcFilename, std::ofstream& dstFile) const
{
	Json::Value jRoot;
	if (!FileLoader::LoadJson(srcFilename.c_str(), jRoot))
	{
		Error("Failed to load scene: %s", srcFilename.c_str());
		return false;
	}

	AssetLib::Scene binData;

	Json::Value jCamera = jRoot.get("camera", Json::Value::null);

	Json::Value jPos = jCamera.get("position", Json::Value::null);
	binData.camPosition = readVec3(jPos);

	Json::Value jRot = jCamera.get("rotation", Json::Value::null);
	binData.camPitchYawRoll = readPitchYawRoll(jRot);

	// Sky
	{
		Json::Value jSky = jRoot.get("sky", Json::Value::null);
		strcpy_s(binData.sky, jSky.asCString());
	}

	// Post-processing effects
	{
		Json::Value jPostProc = jRoot.get("postProcessingEffects", Json::Value::null);
		strcpy_s(binData.postProcessingEffects, jPostProc.asCString());
	}

	// Lights
	{
		Json::Value jLights = jRoot.get("lights", Json::Value::null);

		binData.lightCount = jLights.size();
		binData.lights.ptr = new AssetLib::Light[binData.lightCount];

		for (uint i = 0; i < binData.lightCount; ++i)
		{
			Json::Value jLight = jLights.get(i, Json::Value::null);
			AssetLib::Light& rLight = binData.lights.ptr[i];

			Json::Value jType = jLight.get("type", Json::Value::null);
			const char* typeStr = jType.asCString();
			if (_stricmp(typeStr, "directional") == 0)
			{
				rLight.type = LightType::Directional;

				rLight.direction = readVec3(jLight.get("direction", Json::Value::null));
				rLight.direction = Vec3Normalize(rLight.direction);
			}
			else if (_stricmp(typeStr, "spot") == 0)
			{
				rLight.type = LightType::Spot;

				rLight.direction = readVec3(jLight.get("direction", Json::Value::null));
				rLight.direction = Vec3Normalize(rLight.direction);

				float innerConeAngle = jLight.get("innerConeAngle", 0.f).asFloat();
				rLight.innerConeAngleCos = cosf((innerConeAngle / 180.f) * Maths::kPi);

				float outerConeAngle = jLight.get("outerConeAngle", 0.f).asFloat();
				rLight.outerConeAngleCos = cosf((outerConeAngle / 180.f) * Maths::kPi);
			}
			else if (_stricmp(typeStr, "point") == 0)
			{
				rLight.type = LightType::Point;
			}
			else
			{
				assert(false);
			}

			rLight.position = readVec3(jLight.get("position", Json::Value::null));

			rLight.color = readVec3(jLight.get("color", Json::Value::null));
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

	// Objects
	{
		Json::Value jObjects = jRoot.get("objects", Json::Value::null);

		binData.objectCount = jObjects.size();
		binData.objects.ptr = new AssetLib::Object[binData.objectCount];

		for (uint i = 0; i < binData.objectCount; ++i)
		{
			Json::Value jObj = jObjects.get(i, Json::Value::null);
			AssetLib::Object& rObj = binData.objects.ptr[i];

			rObj.position = readVec3(jObj.get("position", Json::Value::null));
			rObj.orientation = readRotation(jObj.get("rotation", Json::Value::null));
			rObj.scale = readScale(jObj.get("scale", Json::Value::null));

			Json::Value jModel = jObj.get("model", Json::Value::null);
			strcpy_s(rObj.model, jModel.asCString());
		}
	}

	binData.lights.offset = 0;
	binData.objects.offset = binData.lights.offset + binData.lights.CalcSize(binData.lightCount);

	dstFile.write((char*)&binData, sizeof(AssetLib::Scene));
	dstFile.write((char*)binData.lights.ptr, binData.lights.CalcSize(binData.lightCount));
	dstFile.write((char*)binData.objects.ptr, binData.objects.CalcSize(binData.objectCount));

	return true;
}