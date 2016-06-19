#pragma once
#include "AssetDef.h"
#include "BinFile.h"
#include "MathLib/Vec3.h"
#include "MathLib/Quaternion.h"

enum class LightType : int
{
	Directional,
	Point,
	Spot,
	Environment,
};

namespace AssetLib
{
	extern AssetDef g_sceneDef;

	struct Light
	{
		Vec3 position;
		Vec3 direction;
		Vec3 color;
		float radius;

		// Spot light only
		float innerConeAngleCos; // Cosine of angle where light begins to fall off
		float outerConeAngleCos; // No more light

		LightType type;
		bool bCastsShadows;
	};

	struct Object
	{
		Vec3 position;
		Quaternion orientation;
		Vec3 scale;
		char model[AssetLib::AssetDef::kMaxNameLen];
	};

	struct Scene
	{
		static Scene* FromMem(char* pMem);

		char postProcessingEffects[AssetLib::AssetDef::kMaxNameLen];
		char sky[AssetLib::AssetDef::kMaxNameLen];

		Vec3 camPosition;
		Vec3 camPitchYawRoll;
		uint lightCount;
		uint objectCount;

		BinDataPtr<Light> lights;
		BinDataPtr<Object> objects;
	};
}