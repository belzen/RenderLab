#include "Precompiled.h"
#include "RdrGeoSystem.h"
#include "RdrContext.h"
#include "RdrTransientMem.h"
#include <fstream>

namespace
{
	typedef std::map<std::string, RdrGeoHandle> RdrGeoHandleMap;

	struct GeoCmdUpdate
	{
		RdrGeoHandle hGeo;
		const void* pVertData;
		const uint16* pIndexData;
		RdrGeoInfo info;
	};

	struct GeoCmdRelease
	{
		RdrGeoHandle hGeo;
	};

	struct GeoFrameState
	{
		std::vector<GeoCmdUpdate> updates;
		std::vector<GeoCmdRelease> releases;
	};

	struct
	{
		RdrGeoHandleMap geoCache;
		RdrGeoList      geos;

		GeoFrameState states[2];
		uint          queueState;
	} s_geoSystem;

	inline GeoFrameState& getQueueState()
	{
		return s_geoSystem.states[s_geoSystem.queueState];
	}
}

RdrGeoHandle RdrGeoSystem::CreateGeoFromFile(const char* filename, RdrGeoInfo* pOutInfo)
{
	// Find geo in cache
	RdrGeoHandleMap::iterator iter = s_geoSystem.geoCache.find(filename);
	if (iter != s_geoSystem.geoCache.end())
		return iter->second;

	struct ObjVertex
	{
		int iPos;
		int iUv;
		int iNorm;
	};

	static const int kReserveSize = 1024;
	std::vector<ObjVertex> objVerts;
	std::vector<Vertex> verts;
	std::vector<Vec3> positions;
	std::vector<Vec3> normals;
	std::vector<Vec2> texcoords;
	std::vector<uint16> tris;

	char fullFilename[MAX_PATH];
	sprintf_s(fullFilename, "data/geo/%s.obj", filename);
	std::ifstream file(fullFilename);

	char line[1024];
	int lineNum = 0; // for debugging

	while (file.getline(line, 1024))
	{
		++lineNum;

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
			for (int i = 0; i < 3; ++i)
			{
				ObjVertex objVert;
				objVert.iPos = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				objVert.iUv = atoi(strtok_s(nullptr, "/ ", &context)) - 1;
				objVert.iNorm = atoi(strtok_s(nullptr, "/ ", &context)) - 1;

				// Find matching vert to re-use.
				int reuseIndex = -1;
				for (int v = (int)objVerts.size() - 1; v >= 0; --v)
				{
					const ObjVertex& otherVert = objVerts[v];
					if (otherVert.iPos == objVert.iPos && otherVert.iUv == objVert.iUv && otherVert.iNorm == objVert.iNorm)
					{
						reuseIndex = v;
						break;
					}
				}

				if (reuseIndex >= 0)
				{
					tris.push_back(reuseIndex);
				}
				else
				{
					tris.push_back((uint16)verts.size());

					Vertex vert;
					vert.position = positions[objVert.iPos];
					vert.texcoord = texcoords[objVert.iUv];
					vert.normal = normals[objVert.iNorm];
					verts.push_back(vert);

					objVerts.push_back(objVert);
				}
			}
		}
	}

	for (uint i = 0; i < tris.size(); i += 3)
	{
		Vertex& v0 = verts[tris[i + 0]];
		Vertex& v1 = verts[tris[i + 1]];
		Vertex& v2 = verts[tris[i + 2]];

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


	RdrGeometry* pGeo = s_geoSystem.geos.allocSafe();

	GeoCmdUpdate cmd;
	cmd.hGeo = s_geoSystem.geos.getId(pGeo);

	cmd.info.vertStride = sizeof(Vertex);
	cmd.info.numVerts = (int)verts.size();

	Vertex* pVertData = (Vertex*)RdrTransientMem::Alloc(sizeof(Vertex) * cmd.info.numVerts);
	memcpy(pVertData, verts.data(), sizeof(Vertex) * cmd.info.numVerts);
	cmd.pVertData = pVertData;

	cmd.info.numIndices = (int)tris.size();
	uint16* pIndexData = (uint16*)RdrTransientMem::Alloc(sizeof(uint16) * cmd.info.numIndices);
	memcpy(pIndexData, tris.data(), sizeof(uint16) * cmd.info.numIndices);
	cmd.pIndexData = pIndexData;

	// Calc radius
	Vec3 vMin(FLT_MAX, FLT_MAX, FLT_MAX);
	Vec3 vMax(-FLT_MIN, -FLT_MIN, -FLT_MIN);
	for (int i = 0; i < cmd.info.numVerts; ++i)
	{
		vMin.x = min(verts[i].position.x, vMin.x);
		vMax.x = max(verts[i].position.x, vMax.x);
		vMin.y = min(verts[i].position.y, vMin.y);
		vMax.y = max(verts[i].position.y, vMax.y);
		vMin.z = min(verts[i].position.z, vMin.z);
		vMax.z = max(verts[i].position.z, vMax.z);
	}
	cmd.info.size = vMax - vMin;
	cmd.info.center = (vMax + vMin) * 0.5f;
	cmd.info.radius = Vec3Length(cmd.info.size);

	getQueueState().updates.push_back(cmd);

	s_geoSystem.geoCache.insert(std::make_pair(filename, cmd.hGeo));

	if (pOutInfo)
	{
		*pOutInfo = cmd.info;
	}
	return cmd.hGeo;
}

const RdrGeometry* RdrGeoSystem::GetGeo(const RdrGeoHandle hGeo)
{
	return s_geoSystem.geos.get(hGeo);
}

void RdrGeoSystem::ReleaseGeo(const RdrGeoHandle hGeo)
{
	GeoCmdRelease cmd;
	cmd.hGeo = hGeo;

	getQueueState().releases.push_back(cmd);
}

RdrGeoHandle RdrGeoSystem::CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size)
{
	RdrGeometry* pGeo = s_geoSystem.geos.allocSafe();

	GeoCmdUpdate cmd;
	cmd.hGeo = s_geoSystem.geos.getId(pGeo);
	cmd.pVertData = pVertData;
	cmd.pIndexData = pIndexData;
	cmd.info.vertStride = vertStride;
	cmd.info.numVerts = numVerts;
	cmd.info.numIndices = numIndices;
	cmd.info.size = size;
	cmd.info.radius = Vec3Length(size);

	getQueueState().updates.push_back(cmd);

	return cmd.hGeo;
}

void RdrGeoSystem::FlipState()
{
	s_geoSystem.queueState = !s_geoSystem.queueState;
}

void RdrGeoSystem::ProcessCommands(RdrContext* pRdrContext)
{
	GeoFrameState& state = s_geoSystem.states[!s_geoSystem.queueState];
	uint numCmds;

	// Frees
	s_geoSystem.geos.AcquireLock();
	{
		numCmds = (uint)state.releases.size();
		for (uint i = 0; i < numCmds; ++i)
		{
			GeoCmdRelease& cmd = state.releases[i];
			RdrGeometry* pGeo = s_geoSystem.geos.get(cmd.hGeo);

			pRdrContext->ReleaseVertexBuffer(pGeo->vertexBuffer);
			pGeo->vertexBuffer.pBuffer = nullptr;

			pRdrContext->ReleaseIndexBuffer(pGeo->indexBuffer);
			pGeo->indexBuffer.pBuffer = nullptr;

			s_geoSystem.geos.releaseId(cmd.hGeo);
		}
	}
	s_geoSystem.geos.ReleaseLock();

	// Updates
	numCmds = (uint)state.updates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		GeoCmdUpdate& cmd = state.updates[i];
		RdrGeometry* pGeo = s_geoSystem.geos.get(cmd.hGeo);
		if (!pGeo->vertexBuffer.pBuffer)
		{
			// Creating
			pGeo->geoInfo = cmd.info;
			pGeo->vertexBuffer = pRdrContext->CreateVertexBuffer(cmd.pVertData, cmd.info.vertStride * cmd.info.numVerts);
			if (cmd.pIndexData)
			{
				pGeo->indexBuffer = pRdrContext->CreateIndexBuffer(cmd.pIndexData, sizeof(uint16) * cmd.info.numIndices);
			}
		}
		else
		{
			// Updating
			assert(false); //todo
		}
	}

	state.updates.clear();
	state.releases.clear();
}
