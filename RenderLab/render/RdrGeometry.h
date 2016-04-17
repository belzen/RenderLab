#pragma once

#include "RdrResource.h"

struct Vertex
{
	Vec3 position;
	Vec3 normal;
	Color color;
	Vec2 texcoord;
	Vec3 tangent;
	Vec3 bitangent;
};

struct RdrGeoInfo
{
	int numVerts;
	int numIndices;
	uint vertStride;
	Vec3 size;
	float radius;
};

struct RdrVertexBuffer
{
	ID3D11Buffer* pBuffer;
};

struct RdrIndexBuffer
{
	ID3D11Buffer* pBuffer;
};

struct RdrGeometry
{
	RdrVertexBuffer vertexBuffer;
	RdrIndexBuffer indexBuffer;
	RdrGeoInfo geoInfo;
};


//////////////////////////////////////////////////////////////////////////

typedef FreeList<RdrGeometry, 1024> RdrGeoList;
typedef RdrGeoList::Handle RdrGeoHandle;