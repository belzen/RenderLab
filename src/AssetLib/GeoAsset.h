#include "AssetDef.h"
#include "BinFile.h"
#include "MathLib/Vec3.h"
#include "MathLib/Vec2.h"
#include "UtilsLib/Color.h"

namespace GeoAsset
{
	static const uint kBinVersion = 1;
	static const uint kAssetUID = 0x9aa71cdb;

	extern AssetDef Definition;

	struct BinFile
	{
		static BinFile* FromMem(char* pMem);

		Vec3 boundsMin;
		Vec3 boundsMax;
		uint triCount;
		uint vertCount;
		BinDataPtr<Vec3> positions;
		BinDataPtr<Vec2> texcoords;
		BinDataPtr<Vec3> normals;
		BinDataPtr<Color> colors;
		BinDataPtr<Vec3> tangents;
		BinDataPtr<Vec3> bitangents;
		BinDataPtr<uint16> tris;
	};
}