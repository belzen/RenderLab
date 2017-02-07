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
	struct VolumetricFogSettings
	{
		Vec3 scatteringCoeff;
		Vec3 absorptionCoeff;
		float phaseG;
		float farDepth;
		bool enabled;
	};

	struct SkySettings
	{
		VolumetricFogSettings volumetricFog;
	};

	struct PostProcessEffects
	{
		struct
		{
			float white;
			float middleGrey;
			float minExposure;
			float maxExposure;
			float adaptationSpeed;
		} eyeAdaptation;

		struct
		{
			float threshold;
			bool enabled;
		} bloom;

		struct 
		{
			float sampleRadius;
			bool enabled;
		} ssao;
	};

	enum class VolumeType
	{
		kNone,
		kSky,
		kPostProcess
	};

	struct Volume
	{
		VolumeType volumeType;

		PostProcessEffects postProcessEffects;
		SkySettings skySettings;
		// TODO: Volume size;
	};

	struct Light
	{
		LightType type;

		Vec3 direction;
		Vec3 color;
		float intensity;
		float radius;

		// Directional light only
		float pssmLambda;

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

	struct ObjectModel
	{
		CachedString name;
		MaterialSwap materialSwaps[4];
		uint numMaterialSwaps;
	};

	struct ObjectDecal
	{
		CachedString textureName;
	};

	struct Object
	{
		Vec3 position;
		Rotation rotation;
		Vec3 scale;
		ObjectPhysics physics;
		Light light;
		Volume volume;
		ObjectDecal decal;
		ObjectModel model;
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

		Vec3 camPosition;
		Rotation camRotation;

		std::vector<Object> objects;
		Terrain terrain;
		
		Vec3 globalEnvironmentLightPosition;
		uint environmentMapTexSize;
		
		uint timeLastModified;
		CachedString assetName;
	};
}