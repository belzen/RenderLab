#pragma once
#include "AssetDef.h"
#include "BinFile.h"
#include "MathLib/Vec3.h"
#include "MathLib/Vec2.h"
#include "UtilsLib/Color.h"

namespace AssetLib
{
	extern AssetDef g_modelDef;

	struct Model
	{
		static Model* FromMem(char* pMem);

		struct SubObject
		{
			char materialName[AssetDef::kMaxNameLen];
			Vec3 boundsMin;
			Vec3 boundsMax;
			uint indexCount;
			uint vertCount;
		};

		Vec3 boundsMin;
		Vec3 boundsMax;
		uint subObjectCount;
		uint totalIndexCount;
		uint totalVertCount;
		BinDataPtr<SubObject> subobjects;
		BinDataPtr<Vec3> positions;
		BinDataPtr<Vec2> texcoords;
		BinDataPtr<Vec3> normals;
		BinDataPtr<Color> colors;
		BinDataPtr<Vec3> tangents;
		BinDataPtr<Vec3> bitangents;
		BinDataPtr<uint16> indices;
	};
}