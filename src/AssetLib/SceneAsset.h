#pragma once
#include "AssetDef.h"
#include "BinFile.h"
#include "MathLib/Vec2.h"
#include "MathLib/Vec3.h"
#include "MathLib/Quaternion.h"
#include "UtilsLib/StringCache.h"
#include <vector>

enum class LightType : int
{
	None,
	Directional,
	Point,
	Spot,
	Environment,
};

namespace AssetLib
{
	struct Light
	{
		LightType type;

		Vec3 direction;
		Vec3 color;
		float radius;

		// Spot light only
		float innerConeAngle; // Angle where light begins to fall off
		float outerConeAngle; // No more light

		bool bCastsShadows;
		bool bIsGlobalEnvironmentLight;
	};

	struct MaterialSwap
	{
		CachedString from;
		CachedString to;
	};

	enum class ShapeType
	{
		None,
		Box,
		Sphere
	};

	struct ObjectPhysics
	{
		ShapeType shape;
		float density;
		Vec3 offset;
		Vec3 halfSize;
	};

	struct Object
	{
		Vec3 position;
		Quaternion orientation;
		Vec3 scale;
		ObjectPhysics physics;
		Light light;
		MaterialSwap materialSwaps[4];
		uint numMaterialSwaps;
		CachedString modelName;
		char name[64];
	};

	struct Terrain
	{
		Vec2 cornerMin;
		Vec2 cornerMax;
		CachedString heightmapName;
		float heightScale;
		bool enabled;
	};

	struct Scene
	{
		static AssetDef& GetAssetDef();
		static Scene* Load(const CachedString& assetName, Scene* pScene);

		CachedString postProcessingEffectsName;
		CachedString skyName;

		Vec3 camPosition;
		Vec3 camPitchYawRoll;

		std::vector<Object> objects;
		Terrain terrain;
		Vec3 globalEnvironmentLightPosition;

		uint timeLastModified;
		CachedString assetName;
	};
}