#include "Precompiled.h"
#include "RdrContext.h"
#include <assert.h>
#include <stdio.h>
#include <fstream>
#include <vector>

RdrGeoHandle RdrContext::LoadGeo(const char* filename)
{
	RdrGeoMap::iterator iter = m_geoCache.find(filename);
	if (iter != m_geoCache.end())
		return iter->second;


	static const int kReserveSize = 1024;
	std::vector<Vertex> verts;
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> texcoords;
	std::vector<uint16> tris;

	char fullFilename[MAX_PATH];
	sprintf_s(fullFilename, "data/geo/%s", filename);
	std::ifstream file(fullFilename);

	char line[1024];

	while (file.getline(line, 1024))
	{
		char* context = nullptr;
		char* tok = strtok_s(line, " ", &context);

		if (strcmp(tok, "v") == 0)
		{
			// Position
			Vec3 pos;
			pos.x = (float)atof(strtok_s(nullptr, " ", &context));
			pos.y = (float)atof(strtok_s(nullptr, " ", &context));
			pos.z = (float)atof(strtok_s(nullptr, " ", &context));
			positions.push_back(pos);
		}
		else if (strcmp(tok, "vn") == 0)
		{
			// Normal
			Vec3 norm;
			norm.x = (float)atof(strtok_s(nullptr, " ", &context));
			norm.y = (float)atof(strtok_s(nullptr, " ", &context));
			norm.z = (float)atof(strtok_s(nullptr, " ", &context));
			normals.push_back(norm);
		}
		else if (strcmp(tok, "vt") == 0)
		{
			// Tex coord
			Vec2 uv;
			uv.x = (float)atof(strtok_s(nullptr, " ", &context));
			uv.y = (float)atof(strtok_s(nullptr, " ", &context));
			texcoords.push_back(uv);
		}
		else if (strcmp(tok, "f") == 0)
		{
			// Face
			Vec3 tri;
			for (int i = 0; i < 3; ++i)
			{
				int iPos = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				int iUV = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				int iNorm = atoi(strtok_s(nullptr, "/ ", &context)) - 1;

				Vertex vert;
				vert.position = positions[iPos];
				vert.texcoord = texcoords[iUV];
				vert.normal = normals[iNorm];

				tris.push_back((uint16)verts.size());
				verts.push_back(vert);
			}
		}
	}

	for (uint i = 0; i < tris.size(); i += 3)
	{
		Vertex& v0 = verts[tris[i+0]];
		Vertex& v1 = verts[tris[i+1]];
		Vertex& v2 = verts[tris[i+2]];

		Vec3 edge1 = v1.position - v0.position;
		Vec3 edge2 = v2.position - v0.position;

		Vec2 uvEdge1 = v1.texcoord - v0.texcoord;
		Vec2 uvEdge2 = v2.texcoord - v0.texcoord;

		float r = 1.f / (uvEdge1.y * uvEdge2.x - uvEdge1.x * uvEdge2.y);

		Vec3 tangent = (edge1 * -uvEdge2.y + edge2 * uvEdge1.y) * r;
		Vec3 bitangent = (edge1 * -uvEdge2.x + edge2 * uvEdge1.x) * r;

		tangent = Vec3Normalize(tangent);
		bitangent = Vec3Normalize(bitangent);

		v0.tangent = v1.tangent = v2.tangent = tangent;
		v0.bitangent = v1.bitangent = v2.bitangent = bitangent;
	}

	RdrGeometry* pGeo = m_geo.alloc();

	pGeo->numVerts = verts.size();
	pGeo->numIndices = tris.size();
	pGeo->hVertexBuffer = CreateVertexBuffer(verts.data(), sizeof(Vertex) * pGeo->numVerts);
	pGeo->hIndexBuffer = CreateIndexBuffer(tris.data(), sizeof(uint16) * pGeo->numIndices);
	pGeo->vertStride = sizeof(Vertex);

	// Calc radius
	Vec3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3 vMax(-FLT_MIN, -FLT_MIN, -FLT_MIN);
	for (int i = 0; i < pGeo->numVerts; ++i)
	{
		vMin.x = min(verts[i].position.x, vMin.x);
		vMax.x = max(verts[i].position.x, vMax.x);
		vMin.y = min(verts[i].position.y, vMin.y);
		vMax.y = max(verts[i].position.y, vMax.y);
		vMin.z = min(verts[i].position.z, vMin.z);
		vMax.z = max(verts[i].position.z, vMax.z);
	}
	pGeo->size = vMax - vMin;
	pGeo->radius = Vec3Length(pGeo->size);

	RdrGeoHandle hGeo = m_geo.getId(pGeo);
	m_geoCache.insert(std::make_pair(filename, hGeo));
	return hGeo;
}

const RdrGeometry* RdrContext::GetGeometry(RdrGeoHandle hGeo) const
{
	return m_geo.get(hGeo);
}

const RdrResource* RdrContext::GetResource(RdrResourceHandle hRes) const
{
	return m_resources.get(hRes);
}
