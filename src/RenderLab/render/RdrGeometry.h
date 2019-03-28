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
	Point,

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

struct RdrBuffer
{
	ID3D11Buffer* pBuffer;
};

struct RdrGeometry
{
	RdrBuffer vertexBuffer;
	RdrBuffer indexBuffer;
	RdrGeoInfo geoInfo;
};

//////////////////////////////////////////////////////////////////////////

typedef FreeList<RdrGeometry, 1024> RdrGeoList;
typedef RdrGeoList::Handle RdrGeoHandle;


inline bool operator == (const RdrBuffer& rLeft, const RdrBuffer& rRight)
{
	return rLeft.pBuffer == rRight.pBuffer;
}

inline bool operator != (const RdrBuffer& rLeft, const RdrBuffer& rRight)
{
	return !(rLeft == rRight);
}
