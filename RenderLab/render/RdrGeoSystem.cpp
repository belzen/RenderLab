#include "Precompiled.h"
#include "RdrGeoSystem.h"
#include "FileLoader.h"
#include "RdrContext.h"
#include "RdrTransientHeap.h"
#include <fstream>

void RdrGeoSystem::Init(RdrContext* pRdrContext)
{
	m_pRdrContext = pRdrContext;
}

RdrGeoHandle RdrGeoSystem::CreateGeoFromFile(const char* filename, RdrGeoInfo* pOutInfo)
{
	// Find geo in cache
	RdrGeoHandleMap::iterator iter = m_geoCache.find(filename);
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


	RdrGeometry* pGeo = m_geos.alloc();

	CmdUpdate cmd;
	cmd.hGeo = m_geos.getId(pGeo);

	cmd.info.vertStride = sizeof(Vertex);
	cmd.info.numVerts = (int)verts.size();

	Vertex* pVertData = (Vertex*)RdrTransientHeap::Alloc(sizeof(Vertex) * cmd.info.numVerts);
	memcpy(pVertData, verts.data(), sizeof(Vertex) * cmd.info.numVerts);
	cmd.pVertData = pVertData;

	cmd.info.numIndices = (int)tris.size();
	uint16* pIndexData = (uint16*)RdrTransientHeap::Alloc(sizeof(uint16) * cmd.info.numIndices);
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
	cmd.info.radius = Vec3Length(cmd.info.size);

	m_states[m_queueState].updates.push_back(cmd);

	m_geoCache.insert(std::make_pair(filename, cmd.hGeo));

	if (pOutInfo)
	{
		*pOutInfo = cmd.info;
	}
	return cmd.hGeo;
}

const RdrGeometry* RdrGeoSystem::GetGeo(const RdrGeoHandle hGeo)
{
	return m_geos.get(hGeo);
}

const RdrVertexBuffer* RdrGeoSystem::GetVertexBuffer(const RdrVertexBufferHandle hBuffer)
{
	return m_vertexBuffers.get(hBuffer);
}

const RdrIndexBuffer* RdrGeoSystem::GetIndexBuffer(const RdrIndexBufferHandle hBuffer)
{
	return m_indexBuffers.get(hBuffer);
}

void RdrGeoSystem::ReleaseGeo(const RdrGeoHandle hGeo)
{
	CmdRelease cmd;
	cmd.hGeo = hGeo;

	m_states[m_queueState].releases.push_back(cmd);
}

RdrGeoHandle RdrGeoSystem::CreateGeo(const void* pVertData, int vertStride, int numVerts, const uint16* pIndexData, int numIndices, const Vec3& size)
{
	RdrGeometry* pGeo = m_geos.alloc();

	CmdUpdate cmd;
	cmd.hGeo = m_geos.getId(pGeo);
	cmd.pVertData = pVertData;
	cmd.pIndexData = pIndexData;
	cmd.info.vertStride = vertStride;
	cmd.info.numVerts = numVerts;
	cmd.info.numIndices = numIndices;
	cmd.info.size = size;
	cmd.info.radius = Vec3Length(size);

	m_states[m_queueState].updates.push_back(cmd);

	return cmd.hGeo;
}

void RdrGeoSystem::FlipState()
{
	m_queueState = !m_queueState;
}

void RdrGeoSystem::ProcessCommands()
{
	FrameState& state = m_states[!m_queueState];

	// Frees
	uint numCmds = (uint)state.releases.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdRelease& cmd = state.releases[i];
		RdrGeometry* pGeo = m_geos.get(cmd.hGeo);

		RdrVertexBuffer* pVertexBuffer = m_vertexBuffers.get(pGeo->hVertexBuffer);
		m_pRdrContext->ReleaseVertexBuffer(pVertexBuffer);
		m_vertexBuffers.releaseId(pGeo->hVertexBuffer);
		pGeo->hVertexBuffer = 0;

		RdrIndexBuffer* pIndexBuffer = m_indexBuffers.get(pGeo->hIndexBuffer);
		m_pRdrContext->ReleaseIndexBuffer(pIndexBuffer);
		m_indexBuffers.releaseId(pGeo->hIndexBuffer);
		pGeo->hIndexBuffer = 0;

		m_geos.releaseId(cmd.hGeo);
	}

	// Updates
	numCmds = (uint)state.updates.size();
	for (uint i = 0; i < numCmds; ++i)
	{
		CmdUpdate& cmd = state.updates[i];
		RdrGeometry* pGeo = m_geos.get(cmd.hGeo);
		if (!pGeo->hVertexBuffer)
		{
			// Creating
			pGeo->geoInfo = cmd.info;

			RdrVertexBuffer* pVertexBuffer = m_vertexBuffers.alloc();
			*pVertexBuffer = m_pRdrContext->CreateVertexBuffer(cmd.pVertData, cmd.info.vertStride * cmd.info.numVerts);
			pGeo->hVertexBuffer = m_vertexBuffers.getId(pVertexBuffer);

			RdrIndexBuffer* pIndexBuffer = m_indexBuffers.alloc();
			*pIndexBuffer = m_pRdrContext->CreateIndexBuffer(cmd.pIndexData, sizeof(uint16) * cmd.info.numIndices);
			pGeo->hIndexBuffer = m_indexBuffers.getId(pIndexBuffer);
		}
		else
		{
			// Updating
			assert(false); //todo
		}

		RdrTransientHeap::Free(cmd.pVertData);
		RdrTransientHeap::SafeFree(cmd.pIndexData);
	}

	state.updates.clear();
	state.releases.clear();
}
