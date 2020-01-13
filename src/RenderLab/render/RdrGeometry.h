#pragma once

#include "RdrResource.h"
#include "MathLib\Maths.h"
#include "UtilsLib\Color.h"

struct Vertex
{
	Vec3 position;
	Vec3 normal;
	Color32 color;
	Vec2 texcoord;
	Vec2 texcoord1;
	Vec3 tangent;
	Vec3 bitangent;
};

struct RdrGeoInfo
{
	RdrTopology eTopology;
	int numVerts;
	uint vertStride;
	uint nVertexStartByteOffset;
	int numIndices;
	uint nIndexStartByteOffset;
	RdrIndexBufferFormat eIndexFormat;
	Vec3 boundsMin;
	Vec3 boundsMax;
};

struct RdrGeometry
{
	RdrResource* pVertexBuffer;
	RdrResource* pIndexBuffer;
	RdrGeoInfo geoInfo;
	bool bOwnsBuffers;
};

//////////////////////////////////////////////////////////////////////////

typedef FreeList<RdrGeometry, 4096> RdrGeoList;
typedef RdrGeoList::Handle RdrGeoHandle;
