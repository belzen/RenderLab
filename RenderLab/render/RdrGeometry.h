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

typedef uint RdrVertexBufferHandle;
struct RdrVertexBuffer
{
	ID3D11Buffer* pBuffer;
};

typedef uint RdrIndexBufferHandle;
struct RdrIndexBuffer
{
	ID3D11Buffer* pBuffer;
};

typedef uint RdrGeoHandle;
struct RdrGeometry
{
	RdrVertexBufferHandle hVertexBuffer;
	RdrIndexBufferHandle hIndexBuffer;
	RdrGeoInfo geoInfo;
}; 

