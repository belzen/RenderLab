#pragma once
#include "AssetDef.h"
#include "BinFile.h"
#include "MathLib/Vec3.h"
#include "MathLib/Vec2.h"
#include "UtilsLib/Color.h"

namespace ModelAsset
{
	static const uint kBinVersion = 1;
	static const uint kAssetUID = 0x6b64afc;

	extern AssetDef Definition;

	struct BinData
	{
		static BinData* FromMem(char* pMem);

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