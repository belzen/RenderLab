#pragma once

#include "RdrResource.h"
#include "MathLib\Maths.h"
#include "UtilsLib\Color.h"

struct Vertex
{
	Vec3 position;
	Vec3 normal;
	Color color;
	Vec2 texcoord;
	Vec3 tangent;
	Vec3 bitangent;
};

enum class RdrTopology
{
	TriangleList,
	TriangleStrip,
	Quad,

	Count
};

struct RdrGeoInfo
{
	RdrTopology eTopology;
	int numVerts;
	int numIndices;
	uint vertStride;
	Vec3 boundsMin;
	Vec3 boundsMax;
};

struct RdrGeometry
{
	RdrResource vertexBuffer; // donotcheckin - should this be a RdrResource*?
	RdrResource indexBuffer;
	RdrGeoInfo geoInfo;
};

//////////////////////////////////////////////////////////////////////////

typedef FreeList<RdrGeometry, 1024> RdrGeoList;
typedef RdrGeoList::Handle RdrGeoHandle;
