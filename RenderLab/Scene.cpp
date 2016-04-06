#include "Precompiled.h"
#include "Scene.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "WorldObject.h"
#include "render/Model.h"
#include "render/Font.h"
#include "json/json.h"
#include <fstream>
#include <d3d11.h>

static RdrVertexInputElement s_modelVertexDesc[] = {
	{ kRdrShaderSemantic_Position, 0, kRdrVertexInputFormat_RGB_F32, 0, 0, kRdrVertexInputClass_PerVertex, 0 },
	{ kRdrShaderSemantic_Normal, 0, kRdrVertexInputFormat_RGB_F32, 0, 12, kRdrVertexInputClass_PerVertex, 0 },
	{ kRdrShaderSemantic_Color, 0, kRdrVertexInputFormat_RGBA_F32, 0, 24, kRdrVertexInputClass_PerVertex, 0 },
	{ kRdrShaderSemantic_Texcoord, 0, kRdrVertexInputFormat_RG_F32, 0, 40, kRdrVertexInputClass_PerVertex, 0 },
	{ kRdrShaderSemantic_Tangent, 0, kRdrVertexInputFormat_RGB_F32, 0, 48, kRdrVertexInputClass_PerVertex, 0 },
	{ kRdrShaderSemantic_Binormal, 0, kRdrVertexInputFormat_RGB_F32, 0, 60, kRdrVertexInputClass_PerVertex, 0 }
};

Scene::Scene()
{
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
	std::ifstream sceneFile(fullFilename);
	Json::Reader jsonReader;
	jsonReader.parse(sceneFile, root, false);

	// Camera
	{
		Json::Value jCamera = root.get("camera", Json::Value::null);

		Json::Value jPos = jCamera.get("position", Json::Value::null);
		m_mainCamera.SetPosition(readVec3(jPos));

		Json::Value jRot = jCamera.get("rotation", Json::Value::null);
		m_mainCamera.SetPitchYawRoll(readPitchYawRoll(jRot));
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
				light.type = kLightType_Directional;

				light.direction = readVec3(jLight.get("direction", Json::Value::null));
				light.direction = Vec3Normalize(light.direction);
			}
			else if (_stricmp(typeStr, "spot") == 0)
			{
				light.type = kLightType_Spot;

				light.direction = readVec3(jLight.get("direction", Json::Value::null));
				light.direction = Vec3Normalize(light.direction);

				float innerConeAngle = jLight.get("innerConeAngle", 0.f).asFloat();
				light.innerConeAngleCos = cosf((innerConeAngle / 180.f) * Maths::kPi);

				float outerConeAngle = jLight.get("outerConeAngle", 0.f).asFloat();
				light.outerConeAngleCos = cosf((outerConeAngle / 180.f) * Maths::kPi);
			}
			else if (_stricmp(typeStr, "point") == 0)
			{
				light.type = kLightType_Point;
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
			if (light.type == kLightType_Directional)
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

			Json::Value jPixel = jObj.get("pixelShader", Json::Value::null);
			RdrShaderHandle hPixelShader = rRenderer.GetShaderSystem().CreateShaderFromFile(kRdrShaderType_Pixel, jPixel.asCString());
			
			Json::Value jVertex = jObj.get("vertexShader", Json::Value::null);
			RdrShaderHandle hVertexShader = rRenderer.GetShaderSystem().CreateShaderFromFile(kRdrShaderType_Vertex, jVertex.asCString());
			RdrInputLayoutHandle hInputLayout = rRenderer.GetShaderSystem().CreateInputLayout(hVertexShader, s_modelVertexDesc, ARRAYSIZE(s_modelVertexDesc));
			
			Json::Value jModel = jObj.get("model", Json::Value::null);
			RdrGeoHandle hGeo = rRenderer.GetGeoSystem().CreateGeoFromFile(jModel.asCString(), nullptr);

			Json::Value jTextures = jObj.get("textures", Json::Value::null);
			int numTextures = jTextures.size();
			RdrSamplerState samplers[16];
			RdrResourceHandle hTextures[16];
			for (int n = 0; n < numTextures; ++n)
			{
				std::string texName = jTextures.get(n, Json::Value::null).asString();
				hTextures[n] = rRenderer.GetResourceSystem().CreateTextureFromFile(texName.c_str(), nullptr);
				samplers[n] = RdrSamplerState(kComparisonFunc_Never, kRdrTexCoordMode_Wrap, false);
			}

			RdrShaderHandle hCubeMapShader = rRenderer.GetShaderSystem().CreateShaderFromFile(kRdrShaderType_Geometry, "g_cubemap.hlsl");

			// todo: freelists
			Model* pModel = new Model(hGeo, hInputLayout, hVertexShader, hPixelShader, hCubeMapShader, samplers, hTextures, numTextures);
			m_objects.push_back(new WorldObject(pModel, pos, orientation, scale));
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
}
