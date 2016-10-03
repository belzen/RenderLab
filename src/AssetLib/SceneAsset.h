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
		LightType type;

		Vec3 position;
		Vec3 direction;
		Vec3 color;
		float radius;

		// Spot light only
		float innerConeAngle; // Angle where light begins to fall off
		float outerConeAngle; // No more light

		bool bCastsShadows;
	};

	struct Object
	{
		Vec3 position;
		Quaternion orientation;
		Vec3 scale;
		char model[AssetLib::AssetDef::kMaxNameLen];
		char name[64];
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