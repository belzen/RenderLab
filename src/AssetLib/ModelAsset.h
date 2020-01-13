#pragma once
#include "AssetDef.h"
#include "BinFile.h"
#include "MathLib/Vec3.h"
#include "MathLib/Vec2.h"
#include "UtilsLib/Color.h"
#include "UtilsLib/Util.h"

enum class RdrIndexBufferFormat
{
	R16_UINT,
	R32_UINT,

	Count
};

static uint rdrGetIndexBufferFormatSize(RdrIndexBufferFormat eFormat)
{
	return (eFormat == RdrIndexBufferFormat::R16_UINT) ? sizeof(uint16) : sizeof(uint);
}

enum class RdrShaderSemantic
{
	Position,
	Texcoord,
	Color,
	Normal,
	Binormal,
	Tangent,

	Count
};

enum class RdrVertexInputFormat
{
	R_F32,
	RG_F32,
	RGB_F32,
	RGBA_F32,
	RGBA_U8,

	Count
};

enum class RdrVertexInputClass
{
	PerVertex,
	PerInstance,

	Count
};

struct RdrVertexInputElement
{
	RdrShaderSemantic semantic;
	uint semanticIndex;
	RdrVertexInputFormat format;
	uint streamIndex;
	uint byteOffset;
	RdrVertexInputClass inputClass;
	uint instanceDataStepRate;
};


namespace AssetLib
{
	struct Model
	{
		static AssetDef& GetAssetDef();
		static Model* Load(const CachedString& assetName, Model* pModel);

		struct SubObject
		{
			char strMaterialName[AssetDef::kMaxNameLen];
			Vec3 vBoundsMin;
			Vec3 vBoundsMax;
			uint nIndexStartByteOffset;
			uint nIndexCount;
			RdrIndexBufferFormat eIndexFormat;
			uint nVertexStartByteOffset;
			uint nVertexCount;
			uint nVertexStride;
			uint nInputElementStart;
			uint nInputElementCount;
		};

		Vec3 vBoundsMin;
		Vec3 vBoundsMax;
		uint nSubObjectCount;
		uint nTotalInputElementCount;
		uint nTotalIndexCount;
		uint nTotalVertCount;
		uint nIndexBufferSize;
		uint nVertexBufferSize;

		BinDataPtr<SubObject> subobjects;
		BinDataPtr<RdrVertexInputElement> inputElements;
		BinDataPtr<Vec3> positions;
		BinDataPtr<uint8> vertexBuffer;
		BinDataPtr<uint8> indexBuffer;

		uint nTimeLastModified;
		const char* assetName;
	};
}