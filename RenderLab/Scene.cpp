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

static D3D11_INPUT_ELEMENT_DESC s_modelVertexDesc[] = {
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0 }
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

void Scene::Load(RdrContext* pContext, const char* filename)
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
		pContext->m_mainCamera.SetPosition(readVec3(jPos));

		Json::Value jRot = jCamera.get("rotation", Json::Value::null);
		pContext->m_mainCamera.SetPitchYawRoll(readPitchYawRoll(jRot));
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
			}
			else if (_stricmp(typeStr, "spot") == 0)
			{
				light.type = kLightType_Spot;
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

			light.direction = readVec3(jLight.get("direction", Json::Value::null));
			light.direction = Vec3Normalize(light.direction);

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

			float innerConeAngle = jLight.get("innerConeAngle", 0.f).asFloat();
			light.innerConeAngleCos = cosf((innerConeAngle / 180.f) * Maths::kPi);

			float outerConeAngle = jLight.get("outerConeAngle", 0.f).asFloat();
			light.outerConeAngleCos = cosf((outerConeAngle / 180.f) * Maths::kPi);

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
			ShaderHandle hPixelShader = pContext->LoadPixelShader(jPixel.asCString());
			
			Json::Value jVertex = jObj.get("vertexShader", Json::Value::null);
			ShaderHandle hVertexShader = pContext->LoadVertexShader(jVertex.asCString(), s_modelVertexDesc, ARRAYSIZE(s_modelVertexDesc));
			
			Json::Value jModel = jObj.get("model", Json::Value::null);
			RdrGeoHandle hGeo = pContext->LoadGeo(jModel.asCString());

			Json::Value jTextures = jObj.get("textures", Json::Value::null);
			int numTextures = jTextures.size();
			RdrSamplerState samplers[16];
			RdrResourceHandle hTextures[16];
			for (int n = 0; n < numTextures; ++n)
			{
				std::string texName = jTextures.get(n, Json::Value::null).asString();
				hTextures[n] = pContext->LoadTexture(texName.c_str());
				samplers[n] = RdrSamplerState(kComparisonFunc_Never, kRdrTexCoordMode_Wrap, false);
			}

			// todo: freelists
			Model* pModel = new Model(hGeo, hVertexShader, hPixelShader, samplers, hTextures, numTextures);
			m_objects.push_back(new WorldObject(pModel, pos, orientation, scale));
		}
	}

	// TODO: quad/oct tree for scene

	// Spawn a bunch of lights to test forward+
	/*
	for (int x = -150; x < 150; x += 30)
	{
		for (int y = -150; y < 150; y += 30)
		{
			Light light;
			light.type = kLightType_Point;
			light.position.x = (float)x;
			light.position.y = (float)(rand() % 20);
			light.position.z = (float)y;
			light.direction.x = 0.f;
			light.direction.y = -1.f;
			light.direction.z = 0.f;
			light.radius = light.position.y + 5;
			light.color.x = (rand() / (float)RAND_MAX);
			light.color.y = (rand() / (float)RAND_MAX);
			light.color.z = (rand() / (float)RAND_MAX);

			m_lights.AddLight(light);
		}
	}*/
}

void Scene::Update(float dt)
{
}

void Scene::QueueDraw(Renderer& rRenderer)
{
	m_lights.PrepareDraw(rRenderer, rRenderer.GetMainCamera());

	for (uint i = 0; i < m_objects.size(); ++i)
	{
		m_objects[i]->QueueDraw(rRenderer);
	}
}
