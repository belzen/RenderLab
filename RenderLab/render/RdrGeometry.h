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

typedef uint RdrGeoHandle;
struct RdrGeometry
{
	RdrResourceHandle hVertexBuffer;
	RdrResourceHandle hIndexBuffer;
	int numVerts;
	int numIndices;
	uint vertStride;
	Vec3 size;
	float radius;
}; 

